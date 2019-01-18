/****************************************************************************
**			TAU Portable Profiling Package                     **
**			http://www.cs.uoregon.edu/research/tau             **
*****************************************************************************
**    Copyright 2009-2017  						   	   **
**    Department of Computer and Information Science, University of Oregon **
**    Advanced Computing Laboratory, Los Alamos National Laboratory        **
**    Forschungszentrum Juelich                                            **
****************************************************************************/
/****************************************************************************
**	File 		: TracerOTF2.cpp 			        	   **
**	Description 	: TAU Tracing for native OTF2 generation			   **
**  Author      : Nicholas Chaimov
**	Contact		: tau-bugs@cs.uoregon.edu               	   **
**	Documentation	: See http://www.cs.uoregon.edu/research/tau       **
**                                                                         **
**      Description     : TAU Tracing                                      **
**                                                                         **
****************************************************************************/

//#define TAU_OTF2_DEBUG

#define __STDC_FORMAT_MACROS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <dirent.h>

#include <tau_internal.h>
#include <Profile/Profiler.h>
#include <Profile/TauEnv.h>
#include <Profile/TauTrace.h>
#include <Profile/TauTraceOTF2.h>
#include <Profile/TauMetrics.h>
#include <Profile/TauCollectives.h>
#include <Profile/UserEvent.h>

#include <iostream>
#include <sstream>
#include <map>
#include <set>
#include <vector>
#include <utility>
#include <algorithm>

#include <otf2/otf2.h>
#ifdef TAU_OPENMP
#include <otf2/OTF2_OpenMP_Locks.h>
#else
#ifdef PTHREADS
#include <otf2/OTF2_Pthread_Locks.h>
#endif
#endif

#ifdef TAU_INCLUDE_MPI_H_HEADER
#ifdef TAU_MPI
#include <mpi.h>
#endif 
#endif /* TAU_INCLUDE_MPI_H_HEADER */


#define OTF2_EC(call) { \
    OTF2_ErrorCode ec = call; \
    if (ec != OTF2_SUCCESS) { \
        printf("TAU: OTF2 Error (%s:%d): %s, %s\n", __FILE__, __LINE__, OTF2_Error_GetName(ec), OTF2_Error_GetDescription (ec)); \
        abort(); \
    } \
}

using namespace std;
using namespace tau;

static const uint64_t TAU_OTF2_CLOCK_RES = 1000000;

// ID numbers for global OTF2 definition records
static const int TAU_OTF2_COMM_WORLD = 0;
static const int TAU_OTF2_GROUP_LOCS = 0;
static const int TAU_OTF2_GROUP_WORLD = 1;
static const int TAU_OTF2_GROUP_FIRST_AVAILABLE = 2;

static const int TAU_OTF2_COMM_WIN = 0;

static bool otf2_initialized = false;
static bool otf2_comms_shutdown = false;
static bool otf2_finished = false;
static bool otf2_disable = false;
static bool otf2_win_created = false;
static bool otf2_shmem_init = false;
static OTF2_Archive * otf2_archive = NULL;

// Time of first event recorded
static uint64_t start_time = 0;
// Time of last event recorded
static uint64_t end_time = 0;

static uint64_t global_start_time = 0;

struct temp_buffer_entry {
    long int ev;   // Function ID
    x_uint64 ts;   // Timestamp
    x_int64 par;  // Parameter value (1=Enter, -1=Leave)
    int kind;

    temp_buffer_entry(x_uint64 ev, x_uint64 ts, x_uint64 par, int kind)
        : ev(ev), ts(ts), par(par), kind(kind) {};
};

// Temporary buffer for events prior to initialization
// pair.first = FunctionId
// pair.second = timestamp
static vector<temp_buffer_entry> * temp_buffers[TAU_MAX_THREADS] = {0};
static bool buffers_written[TAU_MAX_THREADS] = {0};

// For unification data
static int * num_locations = NULL;
static uint64_t * num_events_written = NULL;
static int * num_regions = NULL;
static int * region_db_sizes = NULL; 
static char * region_names = NULL;
static int * group_db_sizes = NULL;
static char * global_group_names = NULL;
static int * num_metrics = NULL;
static int * metric_db_sizes = NULL;
static char * metric_names = NULL;

// Unification sets the global_region_map on every rank
typedef map<string,uint64_t> region_map_t;
static region_map_t global_region_map;

typedef map<string,set<string> > group_map_t;
static group_map_t global_group_map;

typedef map<string,uint64_t> metric_map_t;
static metric_map_t global_metric_map;

typedef map<string,uint32_t> metric_param_map_t;
static metric_param_map_t global_metric_param_map;

typedef set<uint64_t> metrics_seen_t;
static metrics_seen_t metrics_seen;

extern "C" x_uint64 TauTraceGetTimeStamp(int tid);
extern "C" int tau_totalnodes(int set_or_get, int value);
extern "C" void finalizeCallSites_if_necessary();
extern "C" void Tau_ompt_resolve_callsite(FunctionInfo &fi, char * resolved_address);

// Helper functions

static inline OTF2_LocationRef my_location_offset() {
    const int64_t myNode = RtsLayer::myNode();
    return myNode == -1 ? 0 : (myNode * TAU_MAX_THREADS);
}

static inline OTF2_LocationRef my_location() {
    const int64_t myNode = RtsLayer::myNode();
    const int64_t myThread = RtsLayer::myThread();
    return myNode == -1 ? myThread : (myNode * TAU_MAX_THREADS) + myThread;
}

static inline uint32_t my_node() {
    const int myRtsNode = RtsLayer::myNode();
    const uint32_t my_node = myRtsNode == -1 ? 0 : myRtsNode;
    return my_node;
}

template<class TContainer>
static bool begins_with(const TContainer& input, const TContainer& match)
{
    return input.size() >= match.size()
        && equal(match.begin(), match.end(), input.begin());
}


// Collective Callbacks -- GetSize and GetRank are mandatory
// others are only needed when using SION substrate

static OTF2_CallbackCode tau_collectives_get_size(void*                   userData,
                                                  OTF2_CollectiveContext* commContext,
                                                  uint32_t*               size )
{
  *size = TauCollectives_get_size((TauCollectives_Group*) commContext);
  return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode tau_collectives_get_rank(void*                   userData,
                                                  OTF2_CollectiveContext* commContext,
                                                  uint32_t*               rank )
{
  *rank = my_node();
  return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode
tau_collectives_barrier( void*                   userData,
                              OTF2_CollectiveContext* commContext )
{
    TauCollectives_Barrier((TauCollectives_Group*) commContext );

    return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode
tau_collectives_bcast( void*                   userData,
                            OTF2_CollectiveContext* commContext,
                            void*                   data,
                            uint32_t                numberElements,
                            OTF2_Type               type,
                            uint32_t                root )
{
    TauCollectives_Bcast( ( TauCollectives_Group* )commContext,
                           data,
                           numberElements,
                           TauCollectives_get_type( type ),
                           root );

    return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode
tau_collectives_gather( void*                   userData,
                             OTF2_CollectiveContext* commContext,
                             const void*             inData,
                             void*                   outData,
                             uint32_t                numberElements,
                             OTF2_Type               type,
                             uint32_t                root )
{
    TauCollectives_Gather( ( TauCollectives_Group* )commContext,
                            inData,
                            outData,
                            numberElements,
                            TauCollectives_get_type( type ),
                            root );

    return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode
tau_collectives_gatherv( void*                   userData,
                              OTF2_CollectiveContext* commContext,
                              const void*             inData,
                              uint32_t                inElements,
                              void*                   outData,
                              const uint32_t*         outElements,
                              OTF2_Type               type,
                                         uint32_t                root )
{
    TauCollectives_Gatherv( ( TauCollectives_Group* )commContext,
                             inData,
                             inElements,
                             outData,
                             ( const int* )outElements,
                             TauCollectives_get_type( type ),
                             root );

    return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode
tau_collectives_scatter( void*                   userData,
                              OTF2_CollectiveContext* commContext,
                              const void*             inData,
                              void*                   outData,
                              uint32_t                numberElements,
                              OTF2_Type               type,
                              uint32_t                root )
{
    TauCollectives_Scatter( ( TauCollectives_Group* )commContext,
                             inData,
                             outData,
                             numberElements,
                             TauCollectives_get_type( type ),
                             root );

    return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode
tau_collectives_scatterv( void*                   userData,
                               OTF2_CollectiveContext* commContext,
                               const void*             inData,
                               const uint32_t*         inElements,
                               void*                   outData,
                               uint32_t                outElements,
                               OTF2_Type               type,
                               uint32_t                root )
{
    TauCollectives_Scatterv( ( TauCollectives_Group* )commContext,
                              inData,
                              ( const int* )inElements,
                              outData,
                              outElements,
                              TauCollectives_get_type( type ),
                              root );

    return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CollectiveCallbacks * get_tau_collective_callbacks() {
    static OTF2_CollectiveCallbacks cb;
    cb.otf2_release           = NULL;
    cb.otf2_get_size          = tau_collectives_get_size;
    cb.otf2_get_rank          = tau_collectives_get_rank;
    cb.otf2_create_local_comm = NULL;
    cb.otf2_free_local_comm   = NULL;
    cb.otf2_barrier           = tau_collectives_barrier;
    cb.otf2_bcast             = tau_collectives_bcast;
    cb.otf2_gather            = tau_collectives_gather;
    cb.otf2_gatherv           = tau_collectives_gatherv;
    cb.otf2_scatter           = tau_collectives_scatter;
    cb.otf2_scatterv          = tau_collectives_scatterv;
    return &cb;
}

// Flush Callbacks -- both mandatory

static OTF2_FlushType tau_OTF2PreFlush( void* userData, OTF2_FileType fileType,
        OTF2_LocationRef location, void* callerData, bool final ) {
    return OTF2_FLUSH;
}

static OTF2_TimeStamp tau_OTF2PostFlush(void* userData, OTF2_FileType fileType,
        OTF2_LocationRef location ) { 
    return TauTraceGetTimeStamp(0);
}

static OTF2_FlushCallbacks * get_tau_flush_callbacks() {
   static OTF2_FlushCallbacks cb;
   cb.otf2_pre_flush = tau_OTF2PreFlush;
   cb.otf2_post_flush = tau_OTF2PostFlush;
   return &cb;
}



// Tau Tracing API calls for OTF2

/* Flush the trace buffer */
void TauTraceOTF2FlushBuffer(int tid)
{
  // Protect TAU from itself
  TauInternalFunctionGuard protects_this_function;
}

void TauTraceOTF2InitShmem() {
#ifdef TAU_OTF2_DEBUG
  fprintf(stderr, "TauTraceOTF2InitShmem())\n");
#endif
  otf2_shmem_init = true;    
}
    
/* Initialize tracing. */
int TauTraceOTF2Init(int tid) {
  return TauTraceOTF2InitTS(tid, TauTraceGetTimeStamp(tid));
}

void remove_path(const char *pathname) {
    struct dirent *entry = NULL;
    DIR *dir = NULL;
	struct stat sb;
	if (stat(pathname, &sb) == 0 && S_ISDIR(sb.st_mode)) {
    	dir = opendir(pathname);
    	while((entry = readdir(dir)) != NULL) {
        	DIR *sub_dir = NULL;
        	FILE *file = NULL;
        	char abs_path[4096] = {0};
        	if(*(entry->d_name) != '.') {
            	sprintf(abs_path, "%s/%s", pathname, entry->d_name);
            	sub_dir = opendir(abs_path);
            	if(sub_dir != NULL) {
                	closedir(sub_dir);
                	remove_path(abs_path);
            	} else {
                	file = fopen(abs_path, "r");
                	if(file != NULL) {
                    	fclose(file);
                    	remove(abs_path);
                	}
            	}
        	}
    	}
    	remove(pathname);
	}
}

int TauTraceOTF2InitTS(int tid, x_uint64 ts)
{
  TauInternalFunctionGuard protects_this_function;     
#ifdef TAU_OTF2_DEBUG
  fprintf(stderr, "TauTraceOTF2InitTS(%d, %" PRIu64 ")\n", tid, ts);
#endif
  if(otf2_initialized || otf2_finished) {
      return 0;
  }

#if defined(TAU_MPI)
  // taupreload sets node to 0 when loaded, which results in attempted
  // initialization of the tracing infrastructure. 
  // If MPI_Init hasn't been called, delay OTF2 initialization until it has
  // (at which point we'll be called again)
  int mpi_initialized;
  MPI_Initialized(&mpi_initialized);
  if(!mpi_initialized) {
    return 1;
  }
#endif

#if defined(TAU_SHMEM) && !defined(TAU_MPI)
  if(!otf2_shmem_init) {
    return 1;
  }
#endif
  if(my_node() == 0) {
    const string trace_dir = TauEnv_get_tracedir();
    const string trace_locs_dir = trace_dir + "/traces";
    const string trace_defs = trace_dir + "/traces.def";
    const string trace_anchor = trace_dir + "/traces.otf2";
    remove_path(trace_locs_dir.c_str());
    remove(trace_defs.c_str());
    remove(trace_anchor.c_str());
  }
  otf2_archive = OTF2_Archive_Open(TauEnv_get_tracedir() /* path */,
                             "traces" /* filename */,
                             OTF2_FILEMODE_WRITE,
                             OTF2_CHUNK_SIZE_EVENTS_DEFAULT,
                             OTF2_CHUNK_SIZE_DEFINITIONS_DEFAULT,
                             OTF2_SUBSTRATE_POSIX,
                             OTF2_COMPRESSION_NONE);
  TAU_ASSERT(otf2_archive != NULL, "Unable to create new OTF2 archive");

  OTF2_EC(OTF2_Archive_SetFlushCallbacks(otf2_archive, get_tau_flush_callbacks(), NULL));
  TauCollectives_Init();
  OTF2_EC(OTF2_Archive_SetCollectiveCallbacks(otf2_archive, get_tau_collective_callbacks(), NULL, ( OTF2_CollectiveContext* )TauCollectives_Get_World(), NULL));
  OTF2_EC(OTF2_Archive_SetCreator(otf2_archive, "TAU"));
#if defined(TAU_OPENMP)
#ifdef TAU_OTF2_DEBUG
  fprintf(stderr, "Using OpenMP Locking Callbacks\n");
#endif
  OTF2_EC(OTF2_OpenMP_Archive_SetLockingCallbacks(otf2_archive));
#elif defined(PTHREADS)
#ifdef TAU_OTF2_DEBUG
  fprintf(stderr, "Using Pthread Locking Callbacks\n");
#endif
  OTF2_EC(OTF2_Pthread_Archive_SetLockingCallbacks(otf2_archive, NULL));
#endif
  // If going to use a threading model other than OpenMP or Pthreads,
  // a set of custom locking callbacks will need to be defined.

  OTF2_EC(OTF2_Archive_OpenEvtFiles(otf2_archive));
  OTF2_EC(OTF2_Archive_OpenDefFiles(otf2_archive));

  TAU_ASSERT(evt_writer != NULL, "Failed to open new event writer");

  otf2_initialized = true;
  return 0; 
}

/* This routine is typically invoked when multiple SET_NODE calls are 
   encountered for a multi-threaded program */ 
void TauTraceOTF2Reinitialize(int oldid, int newid, int tid) {
  // TODO find tid's location and call OTF2_EvtWriter_SetLocationID
  return ;
}

/* Reset the trace */
void TauTraceOTF2UnInitialize(int tid) {
  /* to set the trace as uninitialized and clear the current buffers (for forked
     child process, trying to clear its parent records) */
}


/* Write event to buffer */
void TauTraceOTF2EventSimple(long int ev, x_int64 par, int tid, int kind) {
  TauTraceOTF2Event(ev, par, tid, 0, 0, kind);
}

void TauTraceOTF2WriteTempBuffer(int tid, int node_id) {
#ifdef TAU_OTF2_DEBUG
  fprintf(stderr, "TauTraceOTF2WriteTempBuffer(%d, %d)\n", tid, node_id);
#endif
    TauInternalFunctionGuard protects_this_function;
    buffers_written[tid] = true;
    if(temp_buffers[tid] == NULL) {
        return; // Nothing was saved for this thread 
    }
    x_uint64 last_ts = 0;
    for(vector<temp_buffer_entry>::const_iterator it = temp_buffers[tid]->begin(); it != temp_buffers[tid]->end(); ++it) {
      int kind = it->kind == TAU_TRACE_EVENT_KIND_USEREVENT ? TAU_TRACE_EVENT_KIND_TEMP_USEREVENT : TAU_TRACE_EVENT_KIND_TEMP_FUNC;
      TauTraceOTF2EventWithNodeId(it->ev, it->par, tid, it->ts, true, node_id, kind);
      last_ts = it->ts;
    }
    OTF2_EvtWriter* evt_writer = OTF2_Archive_GetEvtWriter(otf2_archive, my_location());
    OTF2_EvtWriter_RmaWinCreate(evt_writer, NULL, last_ts+1, TAU_OTF2_COMM_WIN);
    delete temp_buffers[tid];
}

/* Write event to buffer */
void TauTraceOTF2EventWithNodeId(long int ev, x_int64 par, int tid, x_uint64 ts, int use_ts, int node_id, int kind)
{
  if(otf2_disable) {
    return;
  }
#ifdef TAU_OTF2_DEBUG
  fprintf(stderr, "TauTraceEventWithNodeId(%ld, %" PRId64 ", %d, %" PRIu64 ", %d, %d, %d)\n", ev, par, tid, ts, use_ts, node_id, kind);
#endif
  TauInternalFunctionGuard protects_this_function;
  if(kind == TAU_TRACE_EVENT_KIND_TEMP_FUNC) {
    kind = TAU_TRACE_EVENT_KIND_FUNC;  
  } else if(kind == TAU_TRACE_EVENT_KIND_TEMP_USEREVENT) {
    kind = TAU_TRACE_EVENT_KIND_USEREVENT;
  } else {
    use_ts = false;
  }
  if(otf2_finished) {
    return;
  }
  if(!otf2_initialized) {
    if(start_time == 0) {
      start_time = TauTraceGetTimeStamp(tid) - 1000;   
    }
#if defined(TAU_MPI) || defined(TAU_SHMEM)
    // If we're using MPI, we can't initialize tracing until MPI_Init gets called,
    // which will in turn init tracing for us, so we can't do it here.
    // This is because when we call OTF2_Archive_Open and set the collective callbacks,
    // we must know our rank, because at that time rank 0 alone must create the trace
    // directory. Instead we save into a temporary buffer which we write out as events
    // once initialization happens.
    if(temp_buffers[tid] == NULL) {
        temp_buffers[tid] = new vector<temp_buffer_entry>();
    }
    x_uint64 my_ts = use_ts ? ts : TauTraceGetTimeStamp(tid);
    temp_buffers[tid]->push_back(temp_buffer_entry(ev, my_ts, par, kind));
    return;
#else
    if(use_ts) {
        TauTraceOTF2InitTS(tid, ts); 
    } else {
        TauTraceOTF2Init(tid);
    }
#endif
  }
#if defined(TAU_MPI) || defined(TAU_SHMEM)
  // The event file for a thread needs to be written by that thread, so we write
  // the temporary buffers the first time we get an event from that thread after
  // intialization has completed.
  if(!buffers_written[tid] && !otf2_comms_shutdown) {
    TauTraceOTF2WriteTempBuffer(tid, node_id);
  }
#endif
  int loc = my_location();
  OTF2_EvtWriter* evt_writer = OTF2_Archive_GetEvtWriter(otf2_archive, loc);
  TAU_ASSERT(evt_writer != NULL, "Failed to get event writer");
  x_uint64 my_ts = use_ts ? ts : TauTraceGetTimeStamp(tid);
  if(kind == TAU_TRACE_EVENT_KIND_FUNC || kind == TAU_TRACE_EVENT_KIND_CALLSITE) {
    if(par == 1) { // Enter
      OTF2_EC(OTF2_EvtWriter_Enter(evt_writer, NULL, my_ts, ev));
    } else if(par == -1) { // Exit
      OTF2_EC(OTF2_EvtWriter_Leave(evt_writer, NULL, my_ts, ev));
    }
  } else if(kind == TAU_TRACE_EVENT_KIND_USEREVENT) {
    if(otf2_comms_shutdown && metrics_seen.find(ev) == metrics_seen.end()) {
        // If we've shutdown comms, skip any UserEvents we didn't see before unification
        // (e.g., the fake UserEvents representing metadata)
        return;
    }    
    OTF2_Type types[1] = {OTF2_TYPE_UINT64};
    OTF2_MetricValue values[1];
    values[0].unsigned_int = par;
    OTF2_EC(OTF2_EvtWriter_Metric(evt_writer, NULL, my_ts, ev, 1, types, values))
  }
}


extern "C" void TauTraceOTF2Msg(int send_or_recv, int type, int other_id, int length, x_uint64 ts, int use_ts, int node_id) {
    if(otf2_disable) {
        return;
    }
#ifdef TAU_OTF2_DEBUG                                                                        
  fprintf(stderr, "%d: TauTraceOTF2Msg(%d, %d, %d, %d, %" PRIu64 ", %d, %d)\n", my_node(), send_or_recv, type, other_id, length, ts, use_ts, node_id);
#endif
    TauInternalFunctionGuard protects_this_function;
    if(!otf2_initialized) {
        return;
    }
    x_uint64 time = use_ts ? ts : TauTraceGetTimeStamp(0);
    const int loc = my_location();
    OTF2_EvtWriter* evt_writer = OTF2_Archive_GetEvtWriter(otf2_archive, loc);
#if defined(TAU_SHMEM) && !defined(TAU_MPI)
    const bool remote = (my_node() != node_id);
    if(remote) {
        if(send_or_recv == TAU_MESSAGE_SEND) {
            // A remote send represents the entry to a Get 
            OTF2_EC(OTF2_EvtWriter_RmaGet(evt_writer, NULL, time, TAU_OTF2_COMM_WIN, other_id, length, type));
        } else if(send_or_recv == TAU_MESSAGE_RECV) {
            // A remote recv represents the local completion of a Put
            OTF2_EC(OTF2_EvtWriter_RmaOpCompleteBlocking(evt_writer, NULL, time, TAU_OTF2_COMM_WIN, type));
        }
    } else {
        if(send_or_recv == TAU_MESSAGE_SEND) {
            // A local send represents the entry to a Put
            OTF2_EC(OTF2_EvtWriter_RmaPut(evt_writer, NULL, time, TAU_OTF2_COMM_WIN, other_id, length, type));
        } else if(send_or_recv == TAU_MESSAGE_RECV) {
            // A local recv represents the local completion of a Get
            OTF2_EC(OTF2_EvtWriter_RmaOpCompleteBlocking(evt_writer, NULL, time, TAU_OTF2_COMM_WIN, type));
        }
    }
#else
    if(send_or_recv == TAU_MESSAGE_SEND) {
        OTF2_EC(OTF2_EvtWriter_MpiSend(evt_writer, NULL, time, other_id, TAU_OTF2_COMM_WORLD, type, length));       
    } else if(send_or_recv == TAU_MESSAGE_RECV) {
        OTF2_EC(OTF2_EvtWriter_MpiRecv(evt_writer, NULL, time, other_id, TAU_OTF2_COMM_WORLD, type, length));
    }
#endif
}

/* Write event to buffer */
void TauTraceOTF2Event(long int ev, x_int64 par, int tid, x_uint64 ts, int use_ts, int kind) {
  TauTraceOTF2EventWithNodeId(ev, par, tid, ts, use_ts, my_node(), kind);
}

static void TauTraceOTF2WriteGlobalDefinitions() {
#ifdef TAU_OTF2_DEBUG
  fprintf(stderr, "TauTraceOTF2WriteGlobalDefinitions()\n");
#endif
    TauInternalFunctionGuard protects_this_function;
    OTF2_GlobalDefWriter * global_def_writer = OTF2_Archive_GetGlobalDefWriter(otf2_archive);
    TAU_ASSERT(global_def_writer != NULL, "Failed to get global def writer");
    
    x_uint64 trace_len = end_time - global_start_time;
    global_start_time -= trace_len * 0.02;
    trace_len = end_time - global_start_time;
    trace_len *= 1.02;
    OTF2_GlobalDefWriter_WriteClockProperties(global_def_writer, TAU_OTF2_CLOCK_RES, global_start_time, trace_len);

    // Write a Location for each thread within each Node (which has a LocationGroup and SystemTreeNode)
        
    int nextString = 1;
    const int emptyString = 0;
    OTF2_EC(OTF2_GlobalDefWriter_WriteString(global_def_writer, 0, "" ));

    const int nodes = tau_totalnodes(0, 0);
    for(int node = 0; node < nodes; ++node) {
        // System Tree Node
        char namebuf[256];
        // TODO hostname
        snprintf(namebuf, 256, "node %d", node);                                  
        int nodeName = nextString++;
        OTF2_EC(OTF2_GlobalDefWriter_WriteString(global_def_writer, nodeName, namebuf));
        OTF2_EC(OTF2_GlobalDefWriter_WriteSystemTreeNode(global_def_writer, node, nodeName, emptyString, OTF2_UNDEFINED_SYSTEM_TREE_NODE));        

        // Location Group
        snprintf(namebuf, 256, "group %d", node);
        int groupName = nextString++;
        OTF2_EC(OTF2_GlobalDefWriter_WriteString(global_def_writer, groupName, namebuf));
        OTF2_EC(OTF2_GlobalDefWriter_WriteLocationGroup(global_def_writer, node, groupName, OTF2_LOCATION_GROUP_TYPE_PROCESS, node));

        const int start_loc = node * TAU_MAX_THREADS;
        const int end_loc = start_loc + num_locations[node];
        int thread_num = 0;
        for(int loc = start_loc; loc < end_loc; ++loc) {
            if(nodes < 2 && thread_num == 0) {
                snprintf(namebuf, 256, "Master thread");
            } else if(thread_num == 0) {
                snprintf(namebuf, 256, "Rank");
            } else {
                snprintf(namebuf, 256, "Thread %d", thread_num);
            }
            ++thread_num;
            int locName = nextString++;
            OTF2_EC(OTF2_GlobalDefWriter_WriteString(global_def_writer, locName, namebuf));
            OTF2_EC(OTF2_GlobalDefWriter_WriteLocation(global_def_writer, loc, locName, OTF2_LOCATION_TYPE_CPU_THREAD, num_events_written[node], node));
        }

    }

    // Write all the functions out as Regions
    for (region_map_t::const_iterator it = global_region_map.begin(); it != global_region_map.end(); it++) {
        int thisFuncName = nextString++;
        const std::string & region_name = it->first;
        OTF2_EC(OTF2_GlobalDefWriter_WriteString(global_def_writer, thisFuncName, region_name.c_str()));
        OTF2_EC(OTF2_GlobalDefWriter_WriteRegion(global_def_writer, it->second, thisFuncName, thisFuncName, emptyString, OTF2_REGION_ROLE_FUNCTION, OTF2_PARADIGM_UNKNOWN, OTF2_REGION_FLAG_NONE, 0, 0, 0));
    }

    // Write all the user events out as Metrics
    for (metric_map_t::const_iterator it = global_metric_map.begin(); it != global_metric_map.end(); it++) {
        int thisMetricName = nextString++;
        std::string metric_name = it->first;
        const bool monotonic = metric_name[metric_name.length()-1] == 'M';
        metric_name.erase(metric_name.length()-1);
        OTF2_EC(OTF2_GlobalDefWriter_WriteString(global_def_writer, thisMetricName, metric_name.c_str()));
        const OTF2_MetricMode mode = monotonic ? OTF2_METRIC_ACCUMULATED_START : OTF2_METRIC_ABSOLUTE_POINT;
        const bool papi = metric_name.find("PAPI") != std::string::npos;
        const OTF2_MetricType type = papi ? OTF2_METRIC_TYPE_PAPI : OTF2_METRIC_TYPE_OTHER;
        OTF2_EC(OTF2_GlobalDefWriter_WriteMetricMember(global_def_writer, it->second, thisMetricName, emptyString, type, mode, OTF2_TYPE_UINT64, OTF2_BASE_DECIMAL, 0, emptyString));
        OTF2_MetricMemberRef members[1] = {it->second};
        OTF2_EC(OTF2_GlobalDefWriter_WriteMetricClass(global_def_writer, it->second, 1, members, OTF2_METRIC_SYNCHRONOUS, OTF2_RECORDER_KIND_CPU));
    }

    // Write global communicator
    const int locsGroupName = nextString++;
    OTF2_EC(OTF2_GlobalDefWriter_WriteString(global_def_writer, locsGroupName, "GROUP_MPI_COMM_LOCS"));
    const int worldGroupName = nextString++;
    OTF2_EC(OTF2_GlobalDefWriter_WriteString(global_def_writer, worldGroupName, "GROUP_MPI_COMM_WORLD"));
    const int commName = nextString++;
    OTF2_EC(OTF2_GlobalDefWriter_WriteString(global_def_writer, commName, "MPI_COMM_WORLD"));
    uint64_t nodes_list[nodes];
    uint64_t ranks_list[nodes];
    for(int i = 0; i < nodes; ++i) {
        nodes_list[i] = i * TAU_MAX_THREADS;
        ranks_list[i] = i;
    }
    OTF2_EC(OTF2_GlobalDefWriter_WriteGroup(global_def_writer, TAU_OTF2_GROUP_LOCS, locsGroupName, OTF2_GROUP_TYPE_COMM_LOCATIONS, OTF2_PARADIGM_MPI, OTF2_GROUP_FLAG_NONE, nodes, nodes_list));
    OTF2_EC(OTF2_GlobalDefWriter_WriteGroup(global_def_writer, TAU_OTF2_GROUP_WORLD, worldGroupName, OTF2_GROUP_TYPE_COMM_GROUP, OTF2_PARADIGM_MPI, OTF2_GROUP_FLAG_NONE, nodes, ranks_list));
    OTF2_EC(OTF2_GlobalDefWriter_WriteComm(global_def_writer, TAU_OTF2_COMM_WORLD, commName, TAU_OTF2_GROUP_WORLD, OTF2_UNDEFINED_COMM));
    
    // Write global RMA window
    const int commWinName = nextString++;
    OTF2_EC(OTF2_GlobalDefWriter_WriteString(global_def_writer, commWinName, "RMA_WIN_WORLD"));
    OTF2_EC(OTF2_GlobalDefWriter_WriteRmaWin(global_def_writer, TAU_OTF2_COMM_WIN, commWinName, TAU_OTF2_COMM_WORLD));

    // Write function groups
    int thisGroup = TAU_OTF2_GROUP_FIRST_AVAILABLE - 1;    
    for (group_map_t::const_iterator it = global_group_map.begin(); it != global_group_map.end(); it++) {
        ++thisGroup;
        int thisGroupName = nextString++;
        OTF2_EC(OTF2_GlobalDefWriter_WriteString(global_def_writer, thisGroupName, it->first.c_str()));
        uint32_t num_members = it->second.size();
        uint64_t members[num_members];
        uint32_t i = 0;
        for(set<string>::const_iterator vit = it->second.begin(); vit != it->second.end(); vit++) {
            members[i++] = global_region_map[*vit];    
        }
        OTF2_EC(OTF2_GlobalDefWriter_WriteGroup(global_def_writer, thisGroup, thisGroupName, OTF2_GROUP_TYPE_REGIONS, OTF2_PARADIGM_UNKNOWN, OTF2_GROUP_FLAG_NONE, num_members, members));
    }

    OTF2_EC(OTF2_Archive_CloseGlobalDefWriter(otf2_archive, global_def_writer));
}

static void TauTraceOTF2WriteLocalDefinitions() {
#ifdef TAU_OTF2_DEBUG
  fprintf(stderr, "TauTraceOTF2WriteLocalDefinitions()\n");
#endif
    TauInternalFunctionGuard protects_this_function;

    if(TheFunctionDB().size() > 0) {
        OTF2_IdMap * loc_region_map = OTF2_IdMap_Create(OTF2_ID_MAP_SPARSE, TheFunctionDB().size());
        if(loc_region_map == NULL) {
            fprintf(stderr, "Unable to create OTF2_IdMap for regions of size %zu\n", TheFunctionDB().size());
            abort();
        }
        const region_map_t & global_region_map_ref = global_region_map; 
        for (vector<FunctionInfo*>::iterator it = TheFunctionDB().begin(); it != TheFunctionDB().end(); it++) {
            FunctionInfo * fi = *it;
            const uint64_t local_id  = fi->GetFunctionId();
            const uint64_t global_id = global_region_map_ref.find(string(fi->GetName()))->second;
            OTF2_EC(OTF2_IdMap_AddIdPair(loc_region_map, local_id, global_id));
        }
        const int start_loc = my_location();
        const int end_loc = start_loc + RtsLayer::getTotalThreads();
        for(int loc = start_loc; loc < end_loc; ++loc) {
            OTF2_DefWriter* def_writer = OTF2_Archive_GetDefWriter(otf2_archive, loc);
            OTF2_EC(OTF2_DefWriter_WriteMappingTable(def_writer, OTF2_MAPPING_REGION, loc_region_map));
        }
        OTF2_IdMap_Free(loc_region_map);
    }

    if(TheEventDB().size() > 0) {
        OTF2_IdMap * loc_metric_map = OTF2_IdMap_Create(OTF2_ID_MAP_SPARSE, TheEventDB().size());
        if(loc_metric_map == NULL) {
            fprintf(stderr, "Unable to create OTF2_IdMap for metrics of size %zu\n", TheEventDB().size());
            abort();
        }
        const metric_map_t & global_metric_map_ref = global_metric_map; 
        for (AtomicEventDB::iterator it = TheEventDB().begin(); it != TheEventDB().end(); it++) {
            const uint64_t local_id  = (*it)->GetId();
            bool monotonic = (*it)->IsMonotonicallyIncreasing();
            std::string name = string(((*it)->GetName() + (monotonic ? "M" : "N")).c_str());
            metric_map_t::const_iterator global_id_iter = global_metric_map_ref.find(name);
            if(global_id_iter == global_metric_map_ref.end()) {
                // If this node has metrics that came into existence after comms shutdown,
                // we have nothing to map them to.
                continue;
            }
            const uint64_t global_id = global_id_iter->second;
            OTF2_EC(OTF2_IdMap_AddIdPair(loc_metric_map, local_id, global_id));
        }
        const int start_loc = my_location();
        const int end_loc = start_loc + RtsLayer::getTotalThreads();
        for(int loc = start_loc; loc < end_loc; ++loc) {
            OTF2_DefWriter* def_writer = OTF2_Archive_GetDefWriter(otf2_archive, loc);
            OTF2_EC(OTF2_DefWriter_WriteMappingTable(def_writer, OTF2_MAPPING_METRIC, loc_metric_map));
            OTF2_EC(OTF2_Archive_CloseDefWriter(otf2_archive, def_writer));
        }
        OTF2_IdMap_Free(loc_metric_map);
    }

}

static void TauTraceOTF2ExchangeStartTime() {
#ifdef TAU_OTF2_DEBUG
  fprintf(stderr, "%u: TauTraceOTF2ExchangeStartTime()\n", my_node());
#endif
    TauInternalFunctionGuard protects_this_function;
    if(tau_totalnodes(0, 0) == 1) {
        global_start_time = start_time;
    } else {
        TauCollectives_Reduce(TauCollectives_Get_World(), &start_time, &global_start_time, 1, TAUCOLLECTIVES_UINT64_T, TAUCOLLECTIVES_MIN, 0);
    }
#ifdef TAU_OTF2_DEBUG
    if(my_node() == 0) {
        fprintf(stderr, "Global start time is: %" PRIu64 "\n", global_start_time);
    }
#endif
}

static void TauTraceOTF2ExchangeLocations() {
#ifdef TAU_OTF2_DEBUG
  fprintf(stderr, "%u: TauTraceOTF2ExchangeLocations()\n", my_node());
#endif
    TauInternalFunctionGuard protects_this_function;
    if(my_node() == 0) {
        num_locations = new int[tau_totalnodes(0,0)];
    }
    const uint32_t my_num_threads = RtsLayer::getTotalThreads();
    if(tau_totalnodes(0,0) == 1) {
        num_locations[0] = my_num_threads;
    } else {
        TauCollectives_Gather(TauCollectives_Get_World(), &my_num_threads, num_locations, 1, TAUCOLLECTIVES_UINT32_T, 0);
    }
}

static void TauTraceOTF2ExchangeEventsWritten() {
#ifdef TAU_OTF2_DEBUG
  fprintf(stderr, "%u: TauTraceOTF2ExchangeEventsWritten()\n", my_node());
#endif
    TauInternalFunctionGuard protects_this_function;
    const int nodes = tau_totalnodes(0, 0);
    int total_locs = 0;
    if(my_node() == 0) {
        for(int i = 0; i < nodes; ++i) {
            total_locs += num_locations[i];    
        }
        num_events_written = new uint64_t[total_locs];
    }
    const uint32_t my_num_threads = RtsLayer::getTotalThreads();
    uint64_t my_num_events[my_num_threads];
    const int offset = my_location_offset();
    for(OTF2_LocationRef i = 0; i < my_num_threads; ++i) {
        OTF2_EvtWriter * evt_writer = OTF2_Archive_GetEvtWriter(otf2_archive, offset + i);
        TAU_ASSERT(evt_writer != NULL, "Failed to get event writer");
        OTF2_EC(OTF2_EvtWriter_GetNumberOfEvents(evt_writer, my_num_events + i));
#ifdef TAU_OTF2_DEBUG
        fprintf(stderr, "%u: loc %d has written %u events.\n", my_node(), i, my_num_events[i]);
#endif
    }

    if(nodes == 1) {
        memcpy(num_events_written, my_num_events, my_num_threads);
    } else {
#ifdef TAU_OTF2_DEBUG
        fprintf(stderr, "%u: Calling TauCollectives_Gatherv\n", my_node());
#endif
        TauCollectives_Gatherv(TauCollectives_Get_World(), &my_num_events, my_num_threads, num_events_written, num_locations, TAUCOLLECTIVES_UINT64_T, 0);
#ifdef TAU_OTF2_DEBUG
        fprintf(stderr, "%u: Back from TauCollectives_Gatherv\n", my_node());
#endif
    }

}

static void TauTraceOTF2ExchangeMetrics() {
#ifdef TAU_OTF2_DEBUG
    fprintf(stderr, "%u: TauTraceOTF2ExchangeMetrics()\n", my_node());
#endif
    TauInternalFunctionGuard protects_this_function;

    // Collect local function IDs and names
    const int nodes = tau_totalnodes(0, 0);
    vector<uint64_t> metric_ids;
    stringstream metrics_ss;
    for (AtomicEventDB::iterator uit = TheEventDB().begin(); uit != TheEventDB().end(); uit++) {
        metric_ids.push_back((*uit)->GetId());
        metrics_seen.insert((*uit)->GetId());
        metrics_ss << (*uit)->GetName();
        if((*uit)->IsMonotonicallyIncreasing()) {
            metrics_ss.put('M');
        } else {
            metrics_ss.put('N');
        }
        metrics_ss.put('\0');
    }
    string metric_names_str = metrics_ss.str();
    const char * my_metric_names = metric_names_str.c_str();

    // Gather the number of metrics for each rank on master
    int my_num_metrics = metric_ids.size();
    if(my_node() == 0) {
        num_metrics = new int[nodes];
    }
    if(nodes == 1) {
        num_metrics[0] = my_num_metrics;
    } else {
        TauCollectives_Gather(TauCollectives_Get_World(), &my_num_metrics, num_metrics, 1, TAUCOLLECTIVES_INT, 0);
    }

    // Gather the sizes of the metric name databases on master
    int my_metric_db_size = metric_names_str.size();
    if(my_node() == 0) {
        metric_db_sizes = new int[nodes];
    }
    if(nodes == 1) {
        metric_db_sizes[0] = my_metric_db_size;
    } else {
        TauCollectives_Gather(TauCollectives_Get_World(), &my_metric_db_size, metric_db_sizes, 1, TAUCOLLECTIVES_INT, 0);
    }

    // Exchange metric names
    int total_name_chars = 0;
    if(my_node() == 0) {
        for(int i = 0; i < nodes; ++i) {
            total_name_chars += metric_db_sizes[i];
        }                
        metric_names = new char[total_name_chars];
    
    }
    if(nodes == 1) {
        memcpy(metric_names, my_metric_names, my_metric_db_size);
    } else {
        TauCollectives_Gatherv(TauCollectives_Get_World(), my_metric_names, my_metric_db_size, metric_names, metric_db_sizes, TAUCOLLECTIVES_CHAR, 0);
    }

    // Create and distribute a map of all metric names to global id
    char * global_metrics = NULL;
    int global_metrics_size = 0;
    if(my_node() == 0) {
        int name_offset = 0;
        set<string> unique_names;
        for(int node = 0; node < nodes; ++node) {
            const int node_num_metrics = num_metrics[node];
#ifdef TAU_OTF2_DEBUG
            fprintf(stderr, "Node %d had %d metric types\n", node, node_num_metrics);
#endif
            for(int metric = 0; metric < node_num_metrics; ++metric) {
                string name = string(metric_names+name_offset);    
                name_offset += name.length() + 1;
                unique_names.insert(name);
            }
        }

        int next_id = 0;
        for(set<string>::const_iterator it = unique_names.begin(); it != unique_names.end(); it++) {
            global_metric_map[*it] = next_id++;
        }
        stringstream ss;
        for(metric_map_t::const_iterator it = global_metric_map.begin(); it != global_metric_map.end(); it++) {
            ss << it->first;
            ss.put('\0');
        }
        string global_metrics_str = ss.str();
        global_metrics_size = global_metrics_str.length();
        global_metrics = (char *) malloc(global_metrics_size * sizeof(char));
        global_metrics = (char *) memcpy(global_metrics, global_metrics_str.c_str(), global_metrics_size);    

    }

    if(nodes > 1) {
        TauCollectives_Bcast(TauCollectives_Get_World(), &global_metrics_size, 1, TAUCOLLECTIVES_INT, 0);
    }
    if(my_node() != 0) {
        global_metrics = (char *)malloc(global_metrics_size * sizeof(char));
    }

    if(nodes > 1) {
        TauCollectives_Bcast(TauCollectives_Get_World(), global_metrics, global_metrics_size, TAUCOLLECTIVES_CHAR, 0);
    }

    if(my_node() != 0) {
        int metric_offset = 0;
        int next_id = 0;
        while(metric_offset < global_metrics_size) {
            string name = string(global_metrics+metric_offset);
            metric_offset += name.length() + 1;
            global_metric_map[name] = next_id++;
        }
    }

    free(global_metrics);
        
}

static void TauTraceOTF2ExchangeRegions() {
#ifdef TAU_OTF2_DEBUG
    fprintf(stderr, "%u: TauTraceOTF2ExchangeRegions()\n", my_node());
#endif
    TauInternalFunctionGuard protects_this_function;

    // Collect local function IDs and names
    const int nodes = tau_totalnodes(0, 0);
    vector<uint64_t> function_ids;
    function_ids.reserve(TheFunctionDB().size());
    stringstream names_ss;
    stringstream groups_ss;
    for (vector<FunctionInfo*>::iterator it = TheFunctionDB().begin(); it != TheFunctionDB().end(); it++) {
        char resolved_address[1024] = "";
        FunctionInfo *fi = *it;
        function_ids.push_back(fi->GetFunctionId());
        
        if(strcmp(fi->GetPrimaryGroup(), "TAU_OPENMP") == 0 && !TauEnv_get_ompt_resolve_address_eagerly()) {
          Tau_ompt_resolve_callsite(*fi, resolved_address);
          string temp_ss(resolved_address);
          fi->SetName(temp_ss);
          names_ss << fi->GetName();
        } else {
         names_ss << fi->GetName();
        }
        names_ss.put('\0');

        groups_ss << fi->GetPrimaryGroup();
        groups_ss.put('\0');
    }

    string function_names_str = names_ss.str();
    const char * function_names = function_names_str.c_str();
    string group_names_str = groups_ss.str();
    const char * group_names = group_names_str.c_str();

    // Gather the number of regions for each rank on master
    int my_num_regions = function_ids.size();
    if(my_node() == 0) {
        num_regions = new int[nodes];
    }
    if(nodes == 1) {
        num_regions[0] = my_num_regions;
    } else {
        TauCollectives_Gather(TauCollectives_Get_World(), &my_num_regions, num_regions, 1, TAUCOLLECTIVES_INT, 0);
    }
    
    // Gather the sizes of the function name databases on master
    int my_region_db_size = function_names_str.size();
    if(my_node() == 0) {
        region_db_sizes = new int[nodes];
    }
    if(nodes == 1) {
        region_db_sizes[0] = my_region_db_size;
    } else {
        TauCollectives_Gather(TauCollectives_Get_World(), &my_region_db_size, region_db_sizes, 1, TAUCOLLECTIVES_INT, 0);
    }

    // Exchange names
    int total_name_chars = 0;
    if(my_node() == 0) {
        for(int i = 0; i < nodes; ++i) {
            total_name_chars += region_db_sizes[i];
        }                
        region_names = new char[total_name_chars];
    
    }
    if(nodes == 1) {
        memcpy(region_names, function_names, my_region_db_size);
    } else {
        TauCollectives_Gatherv(TauCollectives_Get_World(), function_names, my_region_db_size, region_names, region_db_sizes, TAUCOLLECTIVES_CHAR, 0);
    }

    // Gather sizes of group names
    int my_group_db_size = group_names_str.size();
    if(my_node() == 0) {
        group_db_sizes = new int[nodes];
    }
    if(nodes == 1) {
        group_db_sizes[0] = my_group_db_size;
    } else {
        TauCollectives_Gather(TauCollectives_Get_World(), &my_group_db_size, group_db_sizes, 1, TAUCOLLECTIVES_INT, 0);
    }

    // Exchange group names
    int total_group_chars = 0;
    if(my_node() == 0) {
        for(int i = 0; i < nodes; ++i) {
            total_group_chars += group_db_sizes[i];
        }                
        global_group_names = new char[total_group_chars];
    
    }
    if(nodes == 1) {
        memcpy(global_group_names, group_names, my_group_db_size);
    } else {
        TauCollectives_Gatherv(TauCollectives_Get_World(), group_names, my_group_db_size, global_group_names, group_db_sizes, TAUCOLLECTIVES_CHAR, 0);
    }

    // Create and distribute a map of all region names to global id
    char * global_regions = NULL;
    int global_regions_size = 0;
    if(my_node() == 0) {
        int name_offset = 0;
        int group_offset = 0;
        set<string> unique_names;
        for(int node = 0; node < nodes; ++node) {
            const int node_num_regions = num_regions[node];
            for(int region = 0; region < node_num_regions; ++region) {
                string name = string(region_names+name_offset);    
                name_offset += name.length() + 1;
                unique_names.insert(name);
                // Node 0 also constructs for itself a map from func name -> group
                string group = string(global_group_names+group_offset);
                group_offset += group.length() + 1;
                global_group_map[group].insert(name);
            }
        }

        int next_id = 0;
        for(set<string>::const_iterator it = unique_names.begin(); it != unique_names.end(); it++) {
            global_region_map[*it] = next_id++;
        }
        stringstream ss;
        for(region_map_t::const_iterator it = global_region_map.begin(); it != global_region_map.end(); it++) {
            ss << it->first;
            ss.put('\0');
        }
        string global_regions_str = ss.str();
        global_regions_size = global_regions_str.length();
        global_regions = (char *) malloc(global_regions_size * sizeof(char));
        global_regions = (char *) memcpy(global_regions, global_regions_str.c_str(), global_regions_size);    
    }

    if(nodes > 1) {
        TauCollectives_Bcast(TauCollectives_Get_World(), &global_regions_size, 1, TAUCOLLECTIVES_INT, 0);
    }
    if(my_node() != 0) {
        global_regions = (char *)malloc(global_regions_size * sizeof(char));
    }

    if(nodes > 1) {
        TauCollectives_Bcast(TauCollectives_Get_World(), global_regions, global_regions_size, TAUCOLLECTIVES_CHAR, 0);
    }

    if(my_node() != 0) {
        int region_offset = 0;
        int next_id = 0;
        while(region_offset < global_regions_size) {
            string name = string(global_regions+region_offset);
            region_offset += name.length() + 1;
            global_region_map[name] = next_id++;
        }
    }

    free(global_regions);

}

void TauTraceOTF2ShutdownComms(int tid) {
#ifdef TAU_OTF2_DEBUG
  fprintf(stderr, "%u: TauTraceOTF2ShutdownComms(%d)\n", my_node(), tid);
#endif
    TauInternalFunctionGuard protects_this_function;
    if(!otf2_initialized || otf2_finished || otf2_comms_shutdown) {
        return;
    }

    const int nodes = tau_totalnodes(0, 0);

    TauCollectives_Barrier(TauCollectives_Get_World());
    otf2_disable = true;
    // Now everyone is at the beginning of MPI_Finalize()
    finalizeCallSites_if_necessary();
    TauTraceOTF2ExchangeStartTime();
    TauTraceOTF2ExchangeLocations();
    TauTraceOTF2ExchangeEventsWritten();
    TauTraceOTF2ExchangeRegions();
    TauTraceOTF2ExchangeMetrics();

    TauCollectives_Finalize();

    otf2_comms_shutdown = true;
    otf2_disable = false;
    end_time = TauTraceGetTimeStamp(0) ;

    // Don't close the trace here -- events can still come in after comms shutdown
    // (in particular, exit from main and exit from .TAU application)
    //TauTraceOTF2Close(tid);
}

/* Close the trace */
void TauTraceOTF2Close(int tid) {
#ifdef TAU_OTF2_DEBUG
  fprintf(stderr, "TauTraceOTF2Close(%d)\n", tid);
#endif
    TauInternalFunctionGuard protects_this_function;
    if(tid != 0 || otf2_finished || !otf2_initialized) {
        return;
    }

    if(!otf2_comms_shutdown) {
        TauTraceOTF2ShutdownComms(tid);
    }

    otf2_finished = true;
    otf2_initialized = false;
    end_time = TauTraceGetTimeStamp(0);

    // Write definitions file
    if(my_node() < 1) {
        TauTraceOTF2WriteGlobalDefinitions();
    }
    TauTraceOTF2WriteLocalDefinitions();
    
    
    OTF2_EC(OTF2_Archive_CloseEvtFiles(otf2_archive));
    OTF2_EC(OTF2_Archive_Close(otf2_archive));

    delete[] num_locations;
    delete[] num_events_written;
    delete[] num_regions;
    delete[] region_db_sizes;
    delete[] region_names;
    delete[] group_db_sizes;
    delete[] global_group_names;
    delete[] num_metrics;
    delete[] metric_db_sizes;
    delete[] metric_names;

}



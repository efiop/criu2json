#include <stdbool.h>
#include <google/protobuf-c/protobuf-c.h>

#include "magic.h"

#include "inventory.pb-c.h"
#include "stats.pb-c.h"
#include "regfile.pb-c.h"
#include "ext-file.pb-c.h"
#include "ns.pb-c.h"
#include "eventfd.pb-c.h"
#include "eventpoll.pb-c.h"
#include "signalfd.pb-c.h"
#include "fsnotify.pb-c.h"
#include "core.pb-c.h"
#include "mm.pb-c.h"
#include "pipe.pb-c.h"
#include "fifo.pb-c.h"
#include "fdinfo.pb-c.h"
#include "pipe-data.pb-c.h"
#include "pstree.pb-c.h"
#include "sa.pb-c.h"
#include "sk-unix.pb-c.h"
#include "sk-inet.pb-c.h"
#include "packet-sock.pb-c.h"
#include "sk-packet.pb-c.h"
#include "creds.pb-c.h"
#include "timer.pb-c.h"
#include "utsns.pb-c.h"
#include "ipc-var.pb-c.h"
#include "ipc-shm.pb-c.h"
#include "ipc-msg.pb-c.h"
#include "ipc-sem.pb-c.h"
#include "fs.pb-c.h"
#include "remap-file-path.pb-c.h"
#include "ghost-file.pb-c.h"
#include "mnt.pb-c.h"
#include "netdev.pb-c.h"
#include "tcp-stream.pb-c.h"
#include "tty.pb-c.h"
#include "file-lock.pb-c.h"
#include "rlimit.pb-c.h"
#include "pagemap.pb-c.h"
#include "siginfo.pb-c.h"
#include "sk-netlink.pb-c.h"
#include "vma.pb-c.h"

#include "tun.pb-c.h"
#include "cgroup.pb-c.h"
#include "timerfd.pb-c.h"

typedef size_t (*pb_getpksize_t)(void *obj);
typedef size_t (*pb_pack_t)(void *obj, void *where);
typedef void  *(*pb_unpack_t)(void *allocator, size_t size, void *from);
typedef void   (*pb_free_t)(void *obj, void *allocator);

struct protobuf_info {
	pb_getpksize_t				getpksize;
	pb_pack_t				pack;
	pb_unpack_t				unpack;
	pb_free_t				free;

	const ProtobufCMessageDescriptor	*desc;
};

/*
 * criu image file structure could be illustrated by:
 * magic pb_size pb pb_size pb ...
 * pb_size is the size of the following protobuf message pb
 */

struct criu_image_info {
	uint32_t		magic;
	bool			is_array;
	struct protobuf_info	header_info;
	struct protobuf_info	extra_info;
};

#define PB_INFO(entry_name) { (pb_getpksize_t)&entry_name##__get_packed_size, (pb_pack_t)&entry_name##__pack, (pb_unpack_t)&entry_name##__unpack, (pb_free_t)&entry_name##__free_unpacked, &entry_name##__descriptor}
#define SINGLE(name, entry_name) { name##_MAGIC, false, PB_INFO(entry_name), NULL } //FIXME do i need to add , after NULL ?
#define ARRAY(name, entry_name) { name##_MAGIC, true, PB_INFO(entry_name), PB_INFO(entry_name) }

struct criu_image_info img_infos [] = {
	SINGLE( INVENTORY,	inventory_entry 	),
	SINGLE( CORE,		core_entry		),
	SINGLE( IDS,		task_kobj_ids_entry	),
	SINGLE( CREDS,		creds_entry		),
	SINGLE( UTSNS,		utsns_entry		),
	SINGLE( IPC_VAR,	ipc_var_entry		),
	SINGLE( FS,		fs_entry		),
	SINGLE( GHOST_FILE,	ghost_file_entry	),
	SINGLE( MM,		mm_entry		),
	SINGLE( CGROUP,		cgroup_entry		),
	SINGLE( TCP_STREAM,	tcp_stream_entry	),
	SINGLE( STATS,		stats_entry		),

	ARRAY( PSTREE,		pstree_entry		),
	ARRAY( REG_FILES,	reg_file_entry		),
	ARRAY( NS_FILES,	ns_file_entry		),
	ARRAY( EVENTFD_FILE,	eventfd_file_entry	),
	ARRAY( EVENTPOLL_FILE,	eventpoll_file_entry	),
	ARRAY( EVENTPOLL_TFD,	eventpoll_tfd_entry	),
	ARRAY( SIGNALFD,	signalfd_entry		),
	ARRAY( TIMERFD,		timerfd_entry		),
	ARRAY( INOTIFY_FILE,	inotify_file_entry	),
	ARRAY( INOTIFY_WD,	inotify_wd_entry	),
	ARRAY( FANOTIFY_FILE,	fanotify_file_entry	),
	ARRAY( FANOTIFY_MARK,	fanotify_mark_entry	),
	ARRAY( VMAS,		vma_entry		),
	ARRAY( PIPES,		pipe_entry		),
	ARRAY( FIFO,		fifo_entry		),
	ARRAY( SIGACT,		sa_entry		),
	ARRAY( NETLINK_SK,	netlink_sk_entry	),
	ARRAY( REMAP_FPATH,	remap_file_path_entry	),
	ARRAY( MNTS,		mnt_entry		),
	ARRAY( TTY_FILES,	tty_file_entry		),
	ARRAY( TTY_INFO,	tty_info_entry		),
	ARRAY( RLIMIT,		rlimit_entry		),
	ARRAY( TUNFILE,		tunfile_entry		),
	ARRAY( EXT_FILES,	ext_file_entry		),
	ARRAY( IRMAP_CACHE,	irmap_cache_entry	),
	ARRAY( FILE_LOCKS,	file_lock_entry		),
	ARRAY( FDINFO,		fdinfo_entry		),
	ARRAY( UNIXSK,		unix_sk_entry		),
	ARRAY( INETSK,		inet_sk_entry		),
	ARRAY( PACKETSK,	packet_sock_entry	),
	ARRAY( ITIMERS,		itimer_entry		),
	ARRAY( POSIX_TIMERS,	posix_timer_entry	),
	ARRAY( NETDEV,		net_device_entry	),
	ARRAY( PIPES_DATA,	pipe_data_entry		),
	ARRAY( FIFO_DATA,	pipe_data_entry		),
	ARRAY( SK_QUEUES,	sk_packet_entry		),
	ARRAY( IPCNS_SHM,	ipc_shm_entry		),
	ARRAY( IPCNS_SEM,	ipc_sem_entry		),
	ARRAY( IPCNS_MSG,	ipc_msg_entry		),

	/*
	 * This one is the special one. It has header pagemap_head
	 * that is followed by an array of pagemap_entry msgs
	 */
	{ PAGEMAP_MAGIC, true, PB_INFO(pagemap_head), PB_INFO(pagemap_entry) },

	{}
};

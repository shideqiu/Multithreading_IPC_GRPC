#include <map>
#include <shared_mutex>
#include <string>
#include <chrono>
#include <mutex>
#include <thread>
#include <errno.h>
#include <iostream>
#include <cstdio>
#include <fstream>
#include <getopt.h>
#include <sys/stat.h>
#include <grpcpp/grpcpp.h>
#include <dirent.h>

#include "src/dfslibx-service-runner.h"
#include "dfslib-shared-p2.h"
#include "proto-src/dfs-service.grpc.pb.h"
#include "src/dfslibx-call-data.h"
#include "dfslib-servernode-p2.h"

using grpc::Status;
using grpc::StatusCode;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerWriter;

using dfs_service::DFSService;

using dfs_service::File;
using dfs_service::StoreRes;
using dfs_service::FetchRequest;
using dfs_service::DeleteRequest;
using dfs_service::DeleteRes;
using dfs_service::ListRequest;
using dfs_service::ListRes;
using dfs_service::StatusRequest;
using dfs_service::StatusRes;
using dfs_service::FileInfo;
using dfs_service::WriteLockRequest;
using dfs_service::WriteLockRes;
using dfs_service::CallbackListRequest;
using dfs_service::CallbackListRes;
//
// STUDENT INSTRUCTION:
//
// Change these "using" aliases to the specific
// message types you are using in your `dfs-service.proto` file
// to indicate a file request and a listing of files from the server
//
using FileRequestType = CallbackListRequest;
using FileListResponseType = CallbackListRes;

extern dfs_log_level_e DFS_LOG_LEVEL;

//
// STUDENT INSTRUCTION:
//
// As with Part 1, the DFSServiceImpl is the implementation service for the rpc methods
// and message types you defined in your `dfs-service.proto` file.
//
// You may start with your Part 1 implementations of each service method.
//
// Elements to consider for Part 2:
//
// - How will you implement the write lock at the server level?
// - How will you keep track of which client has a write lock for a file?
//      - Note that we've provided a preset client_id in DFSClientNode that generates
//        a client id for you. You can pass that to the server to identify the current client.
// - How will you release the write lock?
// - How will you handle a store request for a client that doesn't have a write lock?
// - When matching files to determine similarity, you should use the `file_checksum` method we've provided.
//      - Both the client and server have a pre-made `crc_table` variable to speed things up.
//      - Use the `file_checksum` method to compare two files, similar to the following:
//
//          std::uint32_t server_crc = dfs_file_checksum(filepath, &this->crc_table);
//
//      - Hint: as the crc checksum is a simple integer, you can pass it around inside your message types.
//
class DFSServiceImpl final :
    public DFSService::WithAsyncMethod_CallbackList<DFSService::Service>,
        public DFSCallDataManager<FileRequestType , FileListResponseType> {

private:

    /** The runner service used to start the service and manage asynchronicity **/
    DFSServiceRunner<FileRequestType, FileListResponseType> runner;

    /** Mutex for managing the queue requests **/
    std::mutex queue_mutex;

    std::mutex write_mutex;
    /** The mount path for the server **/
    std::string mount_path;

    /** The vector of queued tags used to manage asynchronous requests **/
    std::vector<QueueRequest<FileRequestType, FileListResponseType>> queued_tags;

    std::map<std::string, std::string> write_map;

    /**
     * Prepend the mount path to the filename.
     *
     * @param filepath
     * @return
     */
    const std::string WrapPath(const std::string &filepath) {
        return this->mount_path + filepath;
    }

    /** CRC Table kept in memory for faster calculations **/
    CRC::Table<std::uint32_t, 32> crc_table;

public:

    DFSServiceImpl(const std::string& mount_path, const std::string& server_address, int num_async_threads):
        mount_path(mount_path), crc_table(CRC::CRC_32()) {

        this->runner.SetService(this);
        this->runner.SetAddress(server_address);
        this->runner.SetNumThreads(num_async_threads);
        this->runner.SetQueuedRequestsCallback([&]{ this->ProcessQueuedRequests(); });

    }

    ~DFSServiceImpl() {
        this->runner.Shutdown();
    }

    void Run() {
        this->runner.Run();
    }

    /**
     * Request callback for asynchronous requests
     *
     * This method is called by the DFSCallData class during
     * an asynchronous request call from the client.
     *
     * Students should not need to adjust this.
     *
     * @param context
     * @param request
     * @param response
     * @param cq
     * @param tag
     */
    void RequestCallback(grpc::ServerContext* context,
                         FileRequestType* request,
                         grpc::ServerAsyncResponseWriter<FileListResponseType>* response,
                         grpc::ServerCompletionQueue* cq,
                         void* tag) {

        std::lock_guard<std::mutex> lock(queue_mutex);
        this->queued_tags.emplace_back(context, request, response, cq, tag);

    }

    /**
     * Process a callback request
     *
     * This method is called by the DFSCallData class when
     * a requested callback can be processed. You should use this method
     * to manage the CallbackList RPC call and respond as needed.
     *
     * See the STUDENT INSTRUCTION for more details.
     *
     * @param context
     * @param request
     * @param response
     */
    void ProcessCallback(ServerContext* context, FileRequestType* request, FileListResponseType* response) {

        //
        // STUDENT INSTRUCTION:
        //
        // You should add your code here to respond to any CallbackList requests from a client.
        // This function is called each time an asynchronous request is made from the client.
        //
        // The client should receive a list of files or modifications that represent the changes this service
        // is aware of. The client will then need to make the appropriate calls based on those changes.
        //
        DIR *dir = opendir(this->mount_path.c_str());
        struct dirent *ent;

        while ((ent= readdir(dir)) != NULL) {
            if (ent->d_type == DT_REG) {
                struct stat buf;
                if (stat(WrapPath(ent->d_name).c_str(), &buf) == -1) {
                    ent = readdir(dir);
                    continue;
                }
                StatusRes* statusRes = response->add_statusres();
                statusRes->set_name(ent->d_name);
                statusRes->set_size(buf.st_size);
                statusRes->set_modifiedtime(buf.st_mtime);
                statusRes->set_createdtime(buf.st_ctime);
                statusRes->set_crc(dfs_file_checksum(WrapPath(ent->d_name), &crc_table));
            }
        }
        closedir(dir);
    }

    /**
     * Processes the queued requests in the queue thread
     */
    void ProcessQueuedRequests() {
        while(true) {

            //
            // STUDENT INSTRUCTION:
            //
            // You should add any synchronization mechanisms you may need here in
            // addition to the queue management. For example, modified files checks.
            //
            // Note: you will need to leave the basic queue structure as-is, but you
            // may add any additional code you feel is necessary.
            //


            // Guarded section for queue
            {
                dfs_log(LL_DEBUG2) << "Waiting for queue guard";
                std::lock_guard<std::mutex> lock(queue_mutex);


                for(QueueRequest<FileRequestType, FileListResponseType>& queue_request : this->queued_tags) {
                    this->RequestCallbackList(queue_request.context, queue_request.request,
                        queue_request.response, queue_request.cq, queue_request.cq, queue_request.tag);
                    queue_request.finished = true;
                }





                // any finished tags first
                this->queued_tags.erase(std::remove_if(
                    this->queued_tags.begin(),
                    this->queued_tags.end(),
                    [](QueueRequest<FileRequestType, FileListResponseType>& queue_request) { return queue_request.finished; }
                ), this->queued_tags.end());

            }
        }
    }

    //
    // STUDENT INSTRUCTION:
    //
    // Add your additional code here, including
    // the implementations of your rpc protocol methods.
    //

    // WriteLock
    Status WriteLock(ServerContext* context,
                     const WriteLockRequest* request,
                     WriteLockRes* response) override {
        if (context->IsCancelled()) {
          return Status(StatusCode::DEADLINE_EXCEEDED, "Deadline handle");
        }
        const std::string client_id = request->clientid();
        const std::string filename = request->name();
        return this->CheckLock(client_id, filename);
    }

    // GetLock
    Status CheckLock(const std::string client_id, const std::string name) {

        std::lock_guard<std::mutex> lock(write_mutex);
        std::cout << "checklock client_id is: " << client_id << std::endl;
        std::cout << "checklock name is: " << name << std::endl;
        std::cout << "checklock write_map is: " << write_map[name] << std::endl;
        if (write_map[name] == "") {
            write_map[name] = client_id;
            return Status::OK;
        } else if (write_map[name] != client_id) {
            return Status(grpc::RESOURCE_EXHAUSTED, "file is locked");
        } else {
            return Status::OK;
        }
    }

    void ReleaseLock(const std::string &name) {
        write_map[name] = "";
    }

     // StoreFile
    Status StoreFile(ServerContext* context,
                     ServerReader<File>* reader,
                     StoreRes* response) override {
         if (context->IsCancelled()) {
           return Status(StatusCode::DEADLINE_EXCEEDED, "Deadline handle");
         }
        File file;
        reader->Read(&file);
        std::cout << "File name: " << file.name() << std::endl;
        std::cout << "File path: " << WrapPath(file.name()) << std::endl;

        // try to get the lock
        Status lock_status = this->CheckLock(file.clientid(), file.name());
        if (!lock_status.ok()) {
            return Status(grpc::RESOURCE_EXHAUSTED, "file is locked");
        }

        // check crc
        std::uint32_t crc = dfs_file_checksum(WrapPath(file.name()), &crc_table);
        if (file.crc() == crc) {
            ReleaseLock(file.name());
            return Status(StatusCode::ALREADY_EXISTS, "already existed");
        }

        // write the file
        std::ofstream outfile(WrapPath(file.name()));
        if (!outfile.is_open()) {
            std::cout << "Cannot open the file" << std::endl;
            ReleaseLock(file.name());
            return Status(StatusCode::CANCELLED, "cancelled");
        }
        outfile.write(file.content().c_str(), file.size());
        while (reader->Read(&file)) {
            outfile.write(file.content().c_str(), file.size());
        }
        ReleaseLock(file.name());
        return Status::OK;
    }

    // FetchFile
    Status FetchFile(ServerContext* context,
                     const FetchRequest* request,
                     ServerWriter<File>* writer) override {
        std::ifstream infile(WrapPath(request->name()));
        std::cout << "File request: " << request << std::endl;
        std::cout << "File name: " << request->name() << std::endl;
        std::cout << "File path: " << WrapPath(request->name()) << std::endl;
        if (!infile) {
            return Status(StatusCode::NOT_FOUND, "file not found");
        }


        if (!infile.is_open()) {
            std::cout << "Cannot open the file" << std::endl;
        }
        File file;
        char buffer[BUFSIZ];
        while (!infile.eof()) {
            if (context->IsCancelled()) {
                return Status(StatusCode::DEADLINE_EXCEEDED, "Deadline handle");
            }
            int bytes_read = infile.read(buffer, BUFSIZ).gcount();
            file.set_name(request->name());
            file.set_size(bytes_read);
            file.set_content(buffer, bytes_read);
            writer->Write(file);
        }
        return Status::OK;
    }

    // DeleteFile
    Status DeleteFile(ServerContext* context,
                      const DeleteRequest* request,
                      DeleteRes* response) override {
        if (context->IsCancelled()) {
          return Status(StatusCode::DEADLINE_EXCEEDED, "Deadline handle");
        }
        std::ifstream infile(WrapPath(request->name()));
        std::cout << "delete file path: " << WrapPath(request->name()) << std::endl;
        std::cout << "delete client_id: " << request->clientid() << std::endl;
        if (!infile) {
            return Status(StatusCode::NOT_FOUND, "file not found");
        } else {
            // try to get the lock
            Status lock_status = CheckLock(request->clientid(), request->name());
            if (!lock_status.ok()) {
                return Status(StatusCode::RESOURCE_EXHAUSTED, "file is locked");
            }
            std::cout << "delete file path ok " << WrapPath(request->name()) << std::endl;
            remove(WrapPath(request->name()).c_str());
            ReleaseLock(request->name());
            return Status::OK;
        }
    }

    // ListFile
    Status ListFile(ServerContext* context,
                    const ListRequest* request,
                    ListRes* response) override {
        if (context->IsCancelled()) {
          return Status(StatusCode::DEADLINE_EXCEEDED, "Deadline handle");
        }
        DIR *dir = opendir(this->mount_path.c_str());
        struct dirent *ent = readdir(dir);
        std::cout << "File path: " << WrapPath(ent->d_name) << std::endl;
        while (ent != NULL) {
            if (ent->d_type == DT_REG) {
                struct stat buf;
                if (stat(WrapPath(ent->d_name).c_str(), &buf) == -1) {
                    ent = readdir(dir);
                    continue;
                }
                FileInfo* fileinfo = response->add_fileinfo();
                fileinfo->set_name(ent->d_name);
                fileinfo->set_modifiedtime(buf.st_mtime);
                std::cout << "File name: " << fileinfo << std::endl;
                std::cout << "ent name: " << ent->d_name << std::endl;
                std::cout << "modified time: " << buf.st_mtime << std::endl;
            }
            ent = readdir(dir);
        }
        closedir(dir);
        return Status::OK;
    }

    // StatusFile
    Status StatusFile(ServerContext* context,
                      const StatusRequest* request,
                      StatusRes* response) override {
        if (context->IsCancelled()) {
          return Status(StatusCode::DEADLINE_EXCEEDED, "Deadline handle");
        }
        struct stat buf;
        if (stat(WrapPath(request->name()).c_str(), &buf) == -1) {
            return Status(StatusCode::NOT_FOUND, "file not found");
        }
        std::uint32_t crc = dfs_file_checksum(WrapPath(request->name()), &crc_table);
        response->set_name(request->name());
        response->set_size(buf.st_size);
        response->set_modifiedtime(buf.st_mtime);
        response->set_createdtime(buf.st_ctime);
        response->set_crc(crc);
        std::cout << "File path is: " << WrapPath(request->name()) << std::endl;
        std::cout << "File name is: " << response->name() << std::endl;
        std::cout << "File size is: " << response->size() << std::endl;
        std::cout << "File mtime is: " << response->modifiedtime() << std::endl;
        std::cout << "File ctime is: " << response->createdtime() << std::endl;
        std::cout << "File crc is: " << response->crc() << std::endl;


        return Status::OK;
    }
};



//
// STUDENT INSTRUCTION:
//
// The following three methods are part of the basic DFSServerNode
// structure. You may add additional methods or change these slightly
// to add additional startup/shutdown routines inside, but be aware that
// the basic structure should stay the same as the testing environment
// will be expected this structure.
//
/**
 * The main server node constructor
 *
 * @param mount_path
 */
DFSServerNode::DFSServerNode(const std::string &server_address,
        const std::string &mount_path,
        int num_async_threads,
        std::function<void()> callback) :
        server_address(server_address),
        mount_path(mount_path),
        num_async_threads(num_async_threads),
        grader_callback(callback) {}
/**
 * Server shutdown
 */
DFSServerNode::~DFSServerNode() noexcept {
    dfs_log(LL_SYSINFO) << "DFSServerNode shutting down";
}

/**
 * Start the DFSServerNode server
 */
void DFSServerNode::Start() {
    DFSServiceImpl service(this->mount_path, this->server_address, this->num_async_threads);


    dfs_log(LL_SYSINFO) << "DFSServerNode server listening on " << this->server_address;
    service.Run();
}

//
// STUDENT INSTRUCTION:
//
// Add your additional definitions here
//

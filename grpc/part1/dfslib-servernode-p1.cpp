#include <chrono>
#include <string>
#include <cstdio>
#include <thread>
#include <iostream>
#include <map>
#include <fstream>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <getopt.h>
#include <grpcpp/grpcpp.h>

#include "src/dfs-utils.h"
#include "dfslib-servernode-p1.h"
#include "proto-src/dfs-service.grpc.pb.h"
#include "dfslib-shared-p1.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::Status;
using grpc::StatusCode;

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
//
// STUDENT INSTRUCTION:
//
// DFSServiceImpl is the implementation service for the rpc methods
// and message types you defined in the `dfs-service.proto` file.
//
// You should add your definition overrides here for the specific
// methods that you created for your GRPC service protocol. The
// gRPC tutorial described in the readme is a good place to get started
// when trying to understand how to implement this class.
//
// The method signatures generated can be found in `proto-src/dfs-service.grpc.pb.h` file.
//
// Look for the following section:
//
//      class Service : public ::grpc::Service {
//
// The methods returning grpc::Status are the methods you'll want to override.
//
// In C++, you'll want to use the `override` directive as well. For example,
// if you have a service method named MyMethod that takes a MyMessageType
// and a ServerWriter, you'll want to override it similar to the following:
//
//      Status MyMethod(ServerContext* context,
//                      const MyMessageType*  request,
//                      ServerWriter<MySegmentType> *writer) override {
//
//          /** code implementation here **/
//      }
//
class DFSServiceImpl final : public DFSService::Service {

private:

    /** The mount path for the server **/
    std::string mount_path;

    /**
     * Prepend the mount path to the filename.
     *
     * @param filepath
     * @return
     */
    const std::string WrapPath(const std::string &filepath) {
        return this->mount_path + filepath;
    }


public:

    DFSServiceImpl(const std::string &mount_path): mount_path(mount_path) {
    }

    ~DFSServiceImpl() {}

    //
    // STUDENT INSTRUCTION:
    //
    // Add your additional code here, including
    // implementations of your protocol service methods
    //

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

        std::ofstream outfile(WrapPath(file.name()));
        if (!outfile.is_open()) {
            std::cout << "Cannot open the file" << std::endl;
        }
        outfile.write(file.content().c_str(), file.size());
        while (reader->Read(&file)) {
            outfile.write(file.content().c_str(), file.size());
        }
        return Status::OK;
    }

    // FetchFile
    Status FetchFile(ServerContext* context,
                     const FetchRequest* request,
                     ServerWriter<File>* writer) override {
        std::ifstream infile(WrapPath(request->name()));
        if (!infile) {
            return Status(StatusCode::NOT_FOUND, "file not found");
        }
        std::cout << "File path: " << WrapPath(request->name()) << std::endl;

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

        if (!infile) {
            return Status(StatusCode::NOT_FOUND, "file not found");
        } else {
            remove(WrapPath(request->name()).c_str());
            return Status::OK;
        }
    }

    // ListFile
    Status ListFile(ServerContext* context,
                    const ListRequest* request,
                    ListRes* response) override {
        DIR *dir = opendir(this->mount_path.c_str());
        struct dirent *ent = readdir(dir);
        if (context->IsCancelled()) {
          return Status(StatusCode::DEADLINE_EXCEEDED, "Deadline handle");
        }
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
        struct stat buf;
        if (context->IsCancelled()) {
          return Status(StatusCode::DEADLINE_EXCEEDED, "Deadline handle");
        }
        if (stat(WrapPath(request->name()).c_str(), &buf) == -1) {
            return Status(StatusCode::NOT_FOUND, "file not found");
        }
        response->set_name(request->name());
        response->set_size(buf.st_size);
        response->set_modifiedtime(buf.st_mtime);
        response->set_createdtime(buf.st_ctime);
        std::cout << "File name is: " << response->name() << std::endl;
        std::cout << "File size is: " << response->size() << std::endl;
        std::cout << "File mtime is: " << response->modifiedtime() << std::endl;
        std::cout << "File ctime is: " << response->createdtime() << std::endl;



return Status::OK;
    }
};

//
// STUDENT INSTRUCTION:
//
// The following three methods are part of the basic DFSServerNode
// structure. You may add additional methods or change these slightly,
// but be aware that the testing environment is expecting these three
// methods as-is.
//
/**
 * The main server node constructor
 *
 * @param server_address
 * @param mount_path
 */
DFSServerNode::DFSServerNode(const std::string &server_address,
        const std::string &mount_path,
        std::function<void()> callback) :
    server_address(server_address), mount_path(mount_path), grader_callback(callback) {}

/**
 * Server shutdown
 */
DFSServerNode::~DFSServerNode() noexcept {
    dfs_log(LL_SYSINFO) << "DFSServerNode shutting down";
    this->server->Shutdown();
}

/** Server start **/
void DFSServerNode::Start() {
    DFSServiceImpl service(this->mount_path);
    ServerBuilder builder;
    builder.AddListeningPort(this->server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    this->server = builder.BuildAndStart();
    dfs_log(LL_SYSINFO) << "DFSServerNode server listening on " << this->server_address;

    this->server->Wait();
}


//
// STUDENT INSTRUCTION:
//
// Add your additional DFSServerNode definitions here
//


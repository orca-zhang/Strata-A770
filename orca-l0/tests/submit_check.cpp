// Same recorded kernels and readback; compare submission/completion APIs only.
// ponytail: one submitter, fixed 64-float workgroups, no SYCL or benchmark dependency.
#include <orca_l0/runtime.hpp>
#include <level_zero/ze_ddi.h>
#include "runtime_dependencies.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>

using namespace orca::l0;
using Clock=std::chrono::steady_clock;
namespace {
void require(bool value,const char* message) { if(!value)throw std::runtime_error(message); }
double us(Clock::time_point a,Clock::time_point b) { return std::chrono::duration<double,std::micro>(b-a).count(); }
struct Handles {
    ze_command_queue_handle_t queue=nullptr;
    ze_command_list_handle_t immediate=nullptr;
    ze_fence_handle_t fence=nullptr;
    ze_event_pool_handle_t pool=nullptr;
    ze_event_handle_t counter=nullptr;
    ~Handles() {
        if(immediate)zeCommandListHostSynchronize(immediate,UINT64_MAX);
        if(queue)zeCommandQueueSynchronize(queue,UINT64_MAX);
        if(counter)zeEventDestroy(counter);
        if(pool)zeEventPoolDestroy(pool);
        if(fence)zeFenceDestroy(fence);
        if(immediate)zeCommandListDestroy(immediate);
        if(queue)zeCommandQueueDestroy(queue);
    }
};
}
int main(int argc,char** argv) try {
    if(argc<2 || argc>3 || (argc==3 && std::strcmp(argv[2],"native")))
        throw std::invalid_argument("usage: submit-check inference.spv|inference.bin [native]");
    std::setvbuf(stdout,nullptr,_IONBF,0);check_runtime_dependencies();
    Runtime rt;auto module=rt.module(argv[1],argc==3?ZE_MODULE_FORMAT_NATIVE:ZE_MODULE_FORMAT_IL_SPIRV);
    auto kernel=rt.kernel(module,"advance");
    uint32_t count=0;check(zeDeviceGetCommandQueueGroupProperties(rt.native_device(),&count,nullptr),"queue count");
    std::vector<ze_command_queue_group_properties_t> groups(count);
    for(auto& g:groups)g.stype=ZE_STRUCTURE_TYPE_COMMAND_QUEUE_GROUP_PROPERTIES;
    check(zeDeviceGetCommandQueueGroupProperties(rt.native_device(),&count,groups.data()),"queue groups");
    uint32_t ordinal=0;while(ordinal<count && !(groups[ordinal].flags&ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE))++ordinal;
    require(ordinal<count,"no compute group");
    Handles h;
    ze_command_queue_desc_t qd{};qd.stype=ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;qd.ordinal=ordinal;
    qd.mode=ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;qd.priority=ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    check(zeCommandQueueCreate(rt.native_context(),rt.native_device(),&qd,&h.queue),"probe queue");
    qd.flags=ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    check(zeCommandListCreateImmediate(rt.native_context(),rt.native_device(),&qd,&h.immediate),"probe immediate");
    auto rejects=[&](auto&& body) {
        bool caught=false;try{body();}catch(const std::invalid_argument&){caught=true;}
        require(caught,"invalid replay batch was accepted");
    };
    ze_command_list_handle_t null_list=nullptr;
    for(size_t count:{size_t(0),size_t(UINT32_MAX)+1}) {
        rejects([&]{replay(h.immediate,count,&null_list);});
        rejects([&]{replay(h.queue,count,&null_list);});
    }
    rejects([&]{replay(h.immediate,1,nullptr);});rejects([&]{replay(h.queue,1,nullptr);});
    rejects([&]{replay(h.immediate,1,&null_list);});rejects([&]{replay(h.queue,1,&null_list);});
    ze_fence_desc_t fd{};fd.stype=ZE_STRUCTURE_TYPE_FENCE_DESC;
    check(zeFenceCreate(h.queue,&fd,&h.fence),"probe fence");
    auto event=rt.event();
    ze_api_version_t version{};check(zeDriverGetApiVersion(rt.native_driver(),&version),"driver API version");
    ze_command_list_exp_dditable_t exp_ddi{};ze_command_list_dditable_t ddi{};
    const bool have_ddi=zeGetCommandListExpProcAddrTable(version,&exp_ddi)==ZE_RESULT_SUCCESS &&
        zeGetCommandListProcAddrTable(version,&ddi)==ZE_RESULT_SUCCESS &&
        exp_ddi.pfnImmediateAppendCommandListsExp && ddi.pfnHostSynchronize;
    std::printf("INFO api_version=%u ddi_available=%d ddi_equals_public_append=%d\n",uint32_t(version),int(have_ddi),
        int(have_ddi && exp_ddi.pfnImmediateAppendCommandListsExp==&zeCommandListImmediateAppendCommandListsExp));
    uint32_t n=0;check(zeDriverGetExtensionProperties(rt.native_driver(),&n,nullptr),"extension count");
    std::vector<ze_driver_extension_properties_t> extensions(n);
    check(zeDriverGetExtensionProperties(rt.native_driver(),&n,extensions.data()),"extensions");
    bool supported=false;for(const auto& e:extensions)if(!std::strcmp(e.name,ZE_EVENT_POOL_COUNTER_BASED_EXP_NAME))supported=true;
    if(supported) {
        ze_event_pool_counter_based_exp_desc_t cb{};cb.stype=ZE_STRUCTURE_TYPE_COUNTER_BASED_EVENT_POOL_EXP_DESC;
        cb.flags=ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE;
        ze_event_pool_desc_t pd{};pd.stype=ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;pd.pNext=&cb;
        pd.flags=ZE_EVENT_POOL_FLAG_HOST_VISIBLE;pd.count=1;auto dev=rt.native_device();
        check(zeEventPoolCreate(rt.native_context(),&pd,1,&dev,&h.pool),"counter pool");
        ze_event_desc_t ed{};ed.stype=ZE_STRUCTURE_TYPE_EVENT_DESC;
        ed.signal=ZE_EVENT_SCOPE_FLAG_HOST|ZE_EVENT_SCOPE_FLAG_DEVICE;ed.wait=ed.signal;
        check(zeEventCreate(h.pool,&ed,&h.counter),"counter event");
    }
    std::printf("INFO device=%s native=%d counter_extension=%d\n",rt.properties().name,argc==3,int(supported));
    if(!supported)std::puts("SKIP counter event: extension not advertised");
    auto* values=static_cast<float*>(rt.allocate(64*4));
    auto* result=static_cast<float*>(rt.allocate(66*4,Memory::host));
    for(int i=0;i<66;++i)result[i]=-12345.f;
    auto& initialize=rt.commands(true,false,true);
    uint32_t zero=0;initialize.fill(values,&zero,4,64*4);require(initialize.wait(),"initialization timeout");
    arguments(kernel,values,uint32_t{1});
    uint32_t expected=0,checks=0;
    for(uint32_t launches:{1u,16u,400u}) {
        auto& graph=rt.commands(Ordering::in_order);
        for(uint32_t i=0;i<launches;++i)graph.launch(kernel,{1,1,1},{64,1,1});
        graph.copy(result+1,values,64*4);graph.close();auto list=graph.native_handle();
        auto run=[&](int mode,bool timed,int repeat) {
            auto begin=Clock::now();
            if(mode==0)replay(h.queue,list);
            else if(mode==1) {
                check(zeFenceReset(h.fence),"fence reset");
                check(zeCommandQueueExecuteCommandLists(h.queue,1,&list,h.fence),"fenced execute");
            } else {
                if(mode==3)check(zeEventHostReset(event),"binary event reset");
                auto append=mode==5?exp_ddi.pfnImmediateAppendCommandListsExp:&zeCommandListImmediateAppendCommandListsExp;
                if(mode==2)replay(h.immediate,list);
                else check(append(h.immediate,1,&list,
                    mode==3?event:mode==4?h.counter:nullptr,0,nullptr),"immediate graph append");
            }
            auto submitted=Clock::now();
            constexpr uint64_t timeout=30000000000ULL;
            if(mode==0)check(zeCommandQueueSynchronize(h.queue,timeout),"queue completion");
            else if(mode==1)check(zeFenceHostSynchronize(h.fence,timeout),"fence completion");
            else if(mode==2 || mode==5)check((mode==5?ddi.pfnHostSynchronize:&zeCommandListHostSynchronize)(h.immediate,timeout),"immediate completion");
            else check(zeEventHostSynchronize(mode==3?event:h.counter,timeout),"event completion");
            auto done=Clock::now();expected+=launches;
            require(expected<16777216,"FP32 exact-check limit");
            require(result[0]==-12345.f && result[65]==-12345.f,"readback guard changed");
            for(int i=1;i<=64;++i)require(result[i]==float(expected),"API completion returned before exact GPU tail");
            ++checks;
            if(timed)std::printf("SAMPLE {\"launches\":%u,\"repeat\":%d,\"mode\":%d,\"submit_us\":%.6f,\"total_us\":%.6f}\n",
                launches,repeat,mode,us(begin,submitted),us(begin,done));
        };
        for(int repeat=0;repeat<4;++repeat)for(int mode=0;mode<6;++mode)
            if((mode!=4 || supported) && (mode!=5 || have_ddi))run(mode,false,repeat);
        for(int repeat=0;repeat<15;++repeat)for(int mode:{0,1,3,4,5}) {
            if((mode==4 && !supported) || (mode==5 && !have_ddi))continue;
            for(int current:{2,mode,mode,2})run(current,true,repeat);
        }
        check(zeCommandListHostSynchronize(h.immediate,UINT64_MAX),"final immediate drain");
        check(zeCommandQueueSynchronize(h.queue,UINT64_MAX),"final queue drain");
        rt.discard(graph);
    }
    std::printf("PASS API submission: %u complete exact GPU tails, guards intact\n",checks);
    uint32_t batch_checks=0;
    for(uint32_t launches:{1u,16u})for(uint32_t lists:{1u,8u,32u}) {
        const size_t size=size_t(lists)*64;
        auto* bank=static_cast<float*>(rt.allocate(size*4));
        auto* output=static_cast<float*>(rt.allocate((size+2)*4,Memory::host));
        output[0]=output[size+1]=-12345.f;
        std::vector<float> initial(size);
        for(size_t i=0;i<size;++i)initial[i]=float((i/64)*17+i%13);
        initialize.copy(bank,initial.data(),size*4);require(initialize.wait(),"batch initialization timeout");
        std::vector<ze_command_list_handle_t> handles;
        std::vector<Commands*> owned;
        for(uint32_t i=0;i<lists;++i) {
            auto& graph=rt.commands(Ordering::in_order);arguments(kernel,bank+size_t(i)*64,uint32_t{1});
            for(uint32_t k=0;k<launches;++k)graph.launch(kernel,{1,1,1},{64,1,1});
            graph.copy(output+1+size_t(i)*64,bank+size_t(i)*64,64*4);graph.close();
            handles.push_back(graph.native_handle());owned.push_back(&graph);
        }
        uint32_t epoch=0;
        auto run=[&](int mode,bool timed,int repeat) {
            auto begin=Clock::now();double submit=0;
            if(mode==2 || mode==3) {
                auto start=Clock::now();
                if(mode==2)replay(h.immediate,lists,handles.data());
                else replay(h.queue,lists,handles.data());
                submit+=us(start,Clock::now());
            } else for(auto list:handles) {
                auto start=Clock::now();
                check(zeCommandListImmediateAppendCommandListsExp(h.immediate,1,&list,nullptr,0,nullptr),"single graph append");
                submit+=us(start,Clock::now());
                if(mode==0)check(zeCommandListHostSynchronize(h.immediate,30000000000ULL),"per-list completion");
            }
            if(mode==3)check(zeCommandQueueSynchronize(h.queue,30000000000ULL),"batch queue completion");
            else if(mode!=0)check(zeCommandListHostSynchronize(h.immediate,30000000000ULL),"batch immediate completion");
            auto done=Clock::now();++epoch;
            require(output[0]==-12345.f && output[size+1]==-12345.f,"batch readback guard changed");
            for(size_t i=0;i<size;++i)require(output[i+1]==initial[i]+float(epoch*launches),"batched list missed,aliased or completed early");
            ++batch_checks;
            if(timed)std::printf("BATCH_SAMPLE {\"launches\":%u,\"lists\":%u,\"repeat\":%d,\"mode\":%d,\"submit_us\":%.6f,\"total_us\":%.6f}\n",
                launches,lists,repeat,mode,submit,us(begin,done));
        };
        for(int repeat=0;repeat<3;++repeat)for(int mode=0;mode<4;++mode)run(mode,false,repeat);
        for(int repeat=0;repeat<7;++repeat)for(int mode:{0,1,3})for(int current:{2,mode,mode,2})run(current,true,repeat);
        for(auto* graph:owned)rt.discard(*graph);
    }
    std::printf("PASS API batching: %u complete exact GPU batches, distinct inputs and guards intact\n",batch_checks);
    check_runtime_dependencies();return 0;
} catch(const std::exception& e) {std::fprintf(stderr,"%s\n",e.what());return 1;}

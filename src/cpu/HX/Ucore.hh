#include "base/types.hh"
#include "cpu/minor/fetch1.hh"
#include "mem/port.hh"
#include "sim/ticked_object.hh"
#include "params/Ucore.hh"
#include <vector>

namespace gem5 {

//Ucore 是整体架构的最顶层，现功能主要是取指
class Ucore: public TickedObject {
        
public:
    Ucore(const UcoreParams &p);
    ~Ucore(); 

    void evaluate() override;
    void startup() override;
    
    /*stopsim()是gem5退出仿真的函数，
    stop()只是让某个object从事件队列中移除*/
    void stopsim();

    int pc;


protected:
    /*ucore 中的线程数，默认为16*/
    unsigned int NumThread;
    /*cacheLineSize,暂定为128*/
    Addr cacheLineSize;

protected:
    /*现在用途不是很明确，因为PC变化固定，后续根据硬件调整*/
    enum FetchState
    {
        FetchHalted,
        FetchWaitingForPC,
        FetchRunning 
    };

    /*用于记录每个线程的取值状态*/
    struct FetchThreadInfo
    {
        // All fields have default initializers.
        FetchThreadInfo () {}

        Addr pc; //例：pc=000001000
        FetchState state=FetchWaitingForPC;

        /*threadId 是ucore内部线程编号，0-15*/
        ThreadID threadId;

        /*streamSeqNum暂未使用*/
        InstSeqNum streamSeqNum=0;
    };

    std::vector<FetchThreadInfo> fetchInfo;

/*下面是为Ucore连接内存端口，具体而言Icache port应该接到L1buffer上*/
protected:
    class Icacheport : public RequestPort
    {
        protected:
            /*owner*/
            Ucore &ucore;

        public:
            /*portId应该和threadId一一对应*/
            PortID portId;
            Icacheport(std::string name, Ucore &ucore,PortID portId):
            RequestPort(name),ucore(ucore),portId(portId)
            { }
        protected:
            bool recvTimingResp(PacketPtr pkt)
            {return ucore.recvTimingResp(pkt); }
            void recvReqRetry() {ucore.recvReqRetry(); }

    };
    Icacheport icacheport;
    bool recvTimingResp(PacketPtr pkt);
    void recvReqRetry();
};
}
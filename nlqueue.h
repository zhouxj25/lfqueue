#ifndef NLQUEUE_H
#define NLQUEUE_H
#include <atomic>
#include <cstdint>
using namespace std;

//structure to hold a pair of head/tail values and other metadata
struct HeadTail {
    HeadTail() : head(0), tail(0), single(true) {}
	volatile atomic<uint32_t> head;  //prod/consumer head
	volatile atomic<uint32_t> tail;  //prod/consumer tail
	bool single;                     //true if single prod/cons
};

#define RING_SP 0x0001
#define RING_SC 0x0002
#define RING_MP 0x0004
#define RING_MC 0x0008

class NoLockRing {
public:
    NoLockRing(uint32_t size, uint32_t flag);

    ~NoLockRing();
    
    int init();
    
    int push(uint32_t size, void *buf);
    
    int push(void *buf);
    
    int pop(uint32_t size, void **buf);
    
    int pop(void **buf);
    
    uint32_t size();
private:
    //aligns input parameter to the next power of 2
    uint32_t align32pow2(uint32_t size);
    //update the prod head for enqueue
    uint32_t updateProdHead(uint32_t size, uint32_t &oldHead, uint32_t &newHead);
    //update the cons head for dequeue
    uint32_t updateConsHead(uint32_t size, uint32_t &oldHead, uint32_t &newHead);
    //prod enqueue functions, the enqueue of pointers on the ring
    void updateProdData(void **data, uint32_t head, uint32_t size);
    //cons dequeue functions, the actuual copy of pointers on the ring to data
    void updateConsData(void **data, uint32_t head, uint32_t size);
    //update the prod tail for enqueue
    void updateProdTail(uint32_t oldHead, uint32_t newHead);
    //update the cons tail for dequeue
    void updateConsTail(uint32_t oldHead, uint32_t newHead);
    //copy pointers functions
    void savePtr(void *dst, void **src, uint32_t dstIndex, uint32_t srcIndex);
    
    void savePtr(void **dst, void *src, uint32_t dstIndex, uint32_t srcIndex);
private:
    uint32_t    m_size; //the size of ring
    uint32_t    m_mask; //the mask of ring
    uint32_t    m_flag; //single or multi prod/cons
    HeadTail    m_prod; //prod head/tail values
    HeadTail    m_cons; //cons head/tail values
    void*       m_buf;  //pointers buffer
    bool        m_is64; //true if 64 bit
};


#endif

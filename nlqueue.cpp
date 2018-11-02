#include "nlqueue.h"
#include <iostream>
#include <chrono>
#include <thread>
using namespace std;

NoLockRing::NoLockRing(uint32_t size, uint32_t flag) : 
    m_size(0), m_flag(flag), m_buf(nullptr), m_is64(true)
{
    m_size = align32pow2(size);
    m_mask = m_size - 1;
    
    if (sizeof(uint64_t) != sizeof(void*))
        m_is64 = false;
}

NoLockRing::~NoLockRing()
{
    if (m_buf)
        free(m_buf);
}
int NoLockRing::init()
{
    m_buf = malloc(m_size * sizeof(void*));
    if (!m_buf)
    {
        cerr << "init ring error[malloc]" << endl;
        return -1;    
    }
    
    if (m_flag & RING_MP)
        m_prod.single = false;
        
    if (m_flag & RING_MC)
        m_cons.single = false;
        
    return 0;
}

int NoLockRing::push(uint32_t size, void *buf)
{
    if (!m_buf)
    {
        cerr << "m_buf is null" << endl;
        return -1;    
    }
    uint32_t oldHead, newHead;
    size = updateProdHead(size, oldHead, newHead);
    if (!size)
        return -1;
        
    updateProdData(&buf, oldHead, size);
    
    updateProdTail(oldHead, newHead);
    
    return size;
}

int NoLockRing::push(void *buf)
{
    return push(1, buf);
}

int NoLockRing::pop(uint32_t size, void **buf)
{
    if (!m_buf)
    {
        cerr << "m_buf is null" << endl;
        return -1;    
    }
    uint32_t oldHead, newHead;
    size = updateConsHead(size, oldHead, newHead);
    if (!size)
        return 0;
        
    updateConsData(buf, oldHead, size);
    
    updateConsTail(oldHead, newHead);
    
    return size;
}

int NoLockRing::pop(void **buf)
{
    return pop(1, buf);
}

uint32_t NoLockRing::size()
{
    return m_size;
}

uint32_t NoLockRing::updateProdHead(uint32_t size, uint32_t &oldHead, uint32_t &newHead)
{
    uint32_t max(size);
    bool success(false);
    do
    {
        oldHead = m_prod.head.load();
        
        size = (m_mask & (m_cons.tail.load() - oldHead)) - 1;
            
        if (0 == size)
            break;
        else if (size > max)
            size = max;
            
        newHead = (oldHead + size) % m_size;
        
        if (m_prod.single)
            m_prod.head = newHead, success = true;
        else
            success = m_prod.head.compare_exchange_weak(oldHead, newHead);
            
    } while (!success);
    
    return size;
}

uint32_t NoLockRing::updateConsHead(uint32_t size, uint32_t &oldHead, uint32_t &newHead)
{
    uint32_t max(size);
    bool success(false);
    do
    {
        oldHead = m_cons.head.load();
        
        size = (m_mask & (m_prod.tail.load() - oldHead));
        if (0 == size)
            break;
        else if (size > max)
            size = max;
            
        newHead = (oldHead + size) % m_size;
        
        if (m_cons.single)
            m_cons.head.store(newHead), success = true;
        else
            success = m_cons.head.compare_exchange_weak(oldHead, newHead);
    }
    while(!success);
    
    return size;
}

void NoLockRing::updateProdData(void **data, uint32_t head, uint32_t size)
{
    uint32_t idx(head);
    uint32_t i(0);
    if (idx + size < m_size)
    {
        for (; i < (size & ~(uint32_t)0x3); i+=4, idx+= 4)
        {
            savePtr(m_buf, data, idx, m_mask & i);
            savePtr(m_buf, data, idx + 1, m_mask & (i + 1));
            savePtr(m_buf, data, idx + 2, m_mask & (i + 2));
            savePtr(m_buf, data, idx + 3, m_mask & (i + 3));
        }
        
        switch (size & 0x3)
        {
            case 3:
                savePtr(m_buf, data, m_mask & idx, i);
                ++idx,++i;
            case 2:
                savePtr(m_buf, data, m_mask & idx, i);
                ++idx,++i;
            case 1:
                savePtr(m_buf, data, m_mask & idx, i);
                ++idx,++i;
        }
    }
    else
    {
        for (; idx < m_size; ++i, ++idx)
            savePtr(m_buf, data, m_mask & idx, i);
        for (idx = 0; i < size; ++i, ++idx)
            savePtr(m_buf, data, m_mask & idx, i);
    }
}

void NoLockRing::updateConsData(void **data, uint32_t head, uint32_t size)
{
    uint32_t idx(head);
    uint32_t i(0);
    if (idx + size < m_size)
    {
        for (; i < (size & ~(uint32_t)0x3); i+=4, idx+= 4)
        {
            savePtr(data, m_buf, i, m_mask & idx);
            savePtr(data, m_buf, i + 1, m_mask & (idx + 1));
            savePtr(data, m_buf, i + 2, m_mask & (idx + 2));
            savePtr(data, m_buf, i + 3, m_mask & (idx + 3));
        }
        
        switch (size & 0x3)
        {
            case 3:
                savePtr(data, m_buf, i, m_mask & idx);
                ++idx,++i;
            case 2:
                savePtr(data, m_buf, i, m_mask & idx);
                ++idx,++i;
            case 1:
                savePtr(data, m_buf, i, m_mask & idx);
                ++idx,++i;
        }
    }
    else
    {
        for (; idx < m_size; ++i, ++idx)
            savePtr(data, m_buf, i, m_mask & idx);
        for (idx = 0; i < size; ++i, ++idx)
            savePtr(data, m_buf, i, m_mask & idx);
    }
}

void NoLockRing::updateProdTail(uint32_t oldHead, uint32_t newHead)
{
    while (m_prod.tail != oldHead)
        this_thread::sleep_for(chrono::microseconds(1));
            
    m_prod.tail.store(newHead);
}

void NoLockRing::updateConsTail(uint32_t oldHead, uint32_t newHead)
{
    while (m_cons.tail != oldHead)
        this_thread::sleep_for(chrono::microseconds(1));
            
    m_cons.tail.store(newHead);
}

uint32_t NoLockRing::align32pow2(uint32_t size)
{
	size--;
	size |= size >> 1;
	size |= size >> 2;
	size |= size >> 4;
	size |= size >> 8;
	size |= size >> 16;

	return size + 1;
}


void NoLockRing::savePtr(void *dst, void **src, uint32_t dstIndex, uint32_t srcIndex)
{
    if (m_is64)
    {
        uint64_t *pdst = reinterpret_cast<uint64_t*>(dst);
        uint64_t **psrc = reinterpret_cast<uint64_t**>(src);
        pdst[dstIndex] = reinterpret_cast<uint64_t>(psrc[srcIndex]); 
    }
    else
    {
        uint32_t *pdst = reinterpret_cast<uint32_t*>(dst);
        uint32_t **psrc = reinterpret_cast<uint32_t**>(src);
        pdst[dstIndex] = reinterpret_cast<uint32_t>(psrc[srcIndex]); 
    }   
}

void NoLockRing::savePtr(void **dst, void *src, uint32_t dstIndex, uint32_t srcIndex)
{
     if (m_is64)
    {
        uint64_t **pdst = reinterpret_cast<uint64_t**>(dst);
        uint64_t *psrc = reinterpret_cast<uint64_t*>(src);
        pdst[dstIndex] = reinterpret_cast<uint64_t*>(psrc[srcIndex]); 
    }
    else
    {
        uint32_t **pdst = reinterpret_cast<uint32_t**>(dst);
        uint32_t *psrc = reinterpret_cast<uint32_t*>(src);
        pdst[dstIndex] = reinterpret_cast<uint32_t*>(psrc[srcIndex]); 
    }  
}

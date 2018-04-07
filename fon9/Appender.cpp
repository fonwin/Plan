﻿/// \file fon9/Appender.cpp
/// \author fonwinz@gmail.com
#include "fon9/Appender.hpp"
#include "fon9/buffer/BufferNodeWaiter.hpp"
#include "fon9/buffer/FwdBufferList.hpp"

namespace fon9 {

Appender::~Appender() {
}
void Appender::MakeCallForWork(WorkContentLocker& lk) {
   if (lk->SetToRinging())
      this->MakeCallNow(lk);
}

bool Appender::WaitNodeConsumed(WorkContentLocker& locker, BufferList& buf) {
   if (locker->WorkingThreadId_ == ThisThread_.ThreadId_) {
      // 到達此處的情況: 處理 ConsumeAppendBuffer(); 時 => 有控制節點要求 flush.
      return false;
   }
   CountDownLatch waiter{1};
   if (BufferNodeWaiter* node = BufferNodeWaiter::Alloc(waiter)) {
      buf.push_back(node);
      if (!this->MakeCallNow(locker))
         return false;
      if (locker.owns_lock())
         locker.unlock();
      waiter.Wait();
      locker.lock();
      return true;
   }
   return false;
}
bool Appender::WaitFlushed(WorkContentLocker& locker) {
   while (!locker->QueuingBuffer_.empty() || locker->WorkingThreadId_) {
      if (!locker->WorkingThreadId_) {
         this->Worker_.TakeCall(locker);
         continue;
      }
      if (!this->WaitNodeConsumed(locker, locker->QueuingBuffer_))
         return false;
   }
   return true;
}
bool Appender::WaitFlushed() {
   bool res;
   this->Worker_.GetWorkContent([this, &res](WorkContentLocker& locker) {
      res = this->WaitFlushed(locker);
   });
   return res;
}
bool Appender::WaitConsumed(WorkContentLocker& locker) {
   return this->WaitNodeConsumed(locker, locker->ConsumedWaiter_);
}

//--------------------------------------------------------------------------//

void Appender::WorkContentController::AddWork(Locker& lk, const void* buf, size_t size) {
   FwdBufferNode* node = FwdBufferNode::CastFrom(lk->QueuingBuffer_.back());
   if (node == nullptr || node->GetRemainSize() < size)
      node = FwdBufferNode::Alloc(size);
   byte* pout = node->GetDataEnd();
   memcpy(pout, buf, size);
   node->SetDataEnd(pout + size);
   if (node != lk->QueuingBuffer_.back())
      lk->QueuingBuffer_.push_back(node);
   this->MakeCallForWork(lk);
}
WorkerState Appender::WorkContentController::TakeCall(Locker& lk) {
   DcQueueList& workingBuffer = lk->UnsafeWorkingBuffer_;
   workingBuffer.push_back(std::move(lk->QueuingBuffer_));
   lk->WorkingThreadId_ = ThisThread_.ThreadId_;
   lk->WorkingNodeCount_ = workingBuffer.GetNodeCount();
   lk.unlock();

   Appender::StaticCast(*this).ConsumeAppendBuffer(workingBuffer);

   lk.lock();
   lk->WorkingNodeCount_ = workingBuffer.GetNodeCount();
   if (fon9_UNLIKELY(!lk->ConsumedWaiter_.empty())) {
      BufferList cbuf{std::move(lk->ConsumedWaiter_)};
      lk.unlock();
      while (BufferNode* cnode = cbuf.pop_front()) {
         if (BufferNodeVirtual* vnode = BufferNodeVirtual::CastFrom(cnode))
            vnode->OnBufferConsumed();
         FreeNode(cnode);
      }
      lk.lock();
   }
   lk->WorkingThreadId_ = ThreadId::IdType{0};
   if (lk->QueuingBuffer_.empty() && lk->WorkingNodeCount_ == 0)
      return WorkerState::Sleeping;
   return WorkerState::Working;
}

} // namespace
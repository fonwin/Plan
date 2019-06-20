﻿// \file fon9/io/TestDevice.cpp
// \author fonwinz@gmail.com
#ifndef __fon9_io_TestDevice_hpp__
#define __fon9_io_TestDevice_hpp__
#include "fon9/io/Device.hpp"
#include "fon9/Log.hpp"

namespace fon9 { namespace io {

fon9_WARN_DISABLE_PADDING;
/// 通常是 UnitTest 拿來測試時使用, 例如: IoFixSession_UT.cpp
class TestDevice : public Device {
   fon9_NON_COPY_NON_MOVE(TestDevice);
   using base = Device;
protected:
   DcQueueList RxBuffer_;
   bool        IsLogEnabled_{true};
   void SetLinkReady(std::string devid) {
      Device::OpThr_SetDeviceId(*this, devid);
      Device::OpThr_SetLinkReady(*this, devid);
   }
   void OpImpl_Open(std::string cfgstr) override {
      fon9_LOG_DEBUG("Device.Open|dev=", ToPtr(this), "|cfg=", cfgstr);
      this->SetLinkReady(cfgstr);
   }
   void OpImpl_Reopen() override {
      std::string devid = this->OpImpl_GetDeviceId();
      fon9_LOG_DEBUG("Device.Reopen|dev=", ToPtr(this), "|devid=", devid);
      this->SetLinkReady(devid);
   }
   void OpImpl_Close(std::string cause) override {
      fon9_LOG_DEBUG("Device.Close|dev=", ToPtr(this), "|devid=", this->OpImpl_GetDeviceId(),
                     "|cause=", cause);
      this->OpImpl_SetState(State::Closed, &cause);
   }

public:
   TestDevice(SessionSP ses, ManagerSP mgr = nullptr, Style style = Style::Simulation, const DeviceOptions* optsDefault = nullptr)
      : base(std::move(ses), std::move(mgr), style, optsDefault) {
   }
   bool IsSendBufferEmpty() const override {
      return true;
   }
   SendResult SendASAP(const void* src, size_t size) override {
      if (this->IsLogEnabled_)
         fon9_LOG_DEBUG("Send|dev=", ToPtr(this), "|", StrView{reinterpret_cast<const char*>(src), size});
      return SendResult{};
   }
   SendResult SendASAP(BufferList&& src) override {
      if (this->IsLogEnabled_)
         fon9_LOG_DEBUG("Send|dev=", ToPtr(this), "|", src);
      size_t srcsz = CalcDataSize(src.cfront());
      DcQueueList{std::move(src)}.PopConsumed(srcsz);
      return SendResult{};
   }
   SendResult SendBuffered(const void* src, size_t size) override {
      return this->SendASAP(src, size);
   }
   SendResult SendBuffered(fon9::BufferList&& src) override {
      return this->SendASAP(std::move(src));
   }
   void OnReceive(const fon9::StrView& str) {
      this->OpQueue_.AddTask(DeviceAsyncOp(str.ToString(), [](Device& dev, std::string rxstr) {
         static_cast<TestDevice*>(&dev)->RxBuffer_.Append(rxstr.c_str(), rxstr.size());
         if (dev.Session_->OnDevice_Recv(dev, static_cast<TestDevice*>(&dev)->RxBuffer_) == RecvBufferSize::NoRecvEvent)
            fon9_LOG_DEBUG("OnReceive.NoRecvEvent|dev=", ToPtr(&dev));
      }));
   }
};
using TestDeviceSP = fon9::intrusive_ptr<TestDevice>;
//--------------------------------------------------------------------------//
/// 直接建立 2 個 device 對接測試.
class TestDevice2 : public TestDevice {
   fon9_NON_COPY_NON_MOVE(TestDevice2);
   using base = TestDevice;
public:
   using PeerSP = fon9::intrusive_ptr<TestDevice2>;
   using base::base;

   void SetPeer(PeerSP peer) {
      this->IsLogEnabled_ = peer->IsLogEnabled_ = false;
      assert(this->Peer_.get() == nullptr && peer->Peer_.get() == nullptr);
      peer->Peer_ = this;
      this->Peer_ = std::move(peer);
   }
   void ResetPeer() {
      if (this->Peer_) {
         this->Peer_->Peer_.reset();
         this->Peer_.reset();
      }
   }

   SendResult SendASAP(const void* src, size_t size) override {
      if (this->Peer_)
         this->Peer_->OnReceive(fon9::StrView(reinterpret_cast<const char*>(src), size));
      return base::SendASAP(src, size);
   }
   SendResult SendASAP(BufferList&& src) override {
      if (!this->Peer_)
         return base::SendASAP(std::move(src));
      DcQueueList dcq{std::move(src)};
      while (!dcq.empty()) {
         auto blk = dcq.PeekCurrBlock();
         this->Peer_->OnReceive(fon9::StrView(reinterpret_cast<const char*>(blk.first), blk.second));
         dcq.PopConsumed(blk.second);
      }
      return SendResult{};
   }

protected:
   PeerSP   Peer_;
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_TestDevice_hpp__
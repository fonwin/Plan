﻿/// \file fon9/ObjPool.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_ObjPool_hpp__
#define __fon9_ObjPool_hpp__
#include "fon9/sys/Config.hpp"

fon9_BEFORE_INCLUDE_STD;
#include <algorithm>
fon9_AFTER_INCLUDE_STD;

namespace fon9 {

/// \ingroup Misc
/// - 透過一個陣列重複使用裡面的物件.
/// - 使用 Add(), Alloc() 取得 pool 裡面的物件索引.
/// - 當需要使用物件時, 用 GetObjPtr(idx); 取得物件指標, 或 GetObj() 取得物件副本.
/// - 當物件不再需要時, 用 RemoveObj(); RemoveObjPtr(); 清除物件資源.
///   - 但物件仍然存在沒有被解構.
template <class T>
class ObjPool {
public:
   using ContainerT = std::vector<T>;
   using SizeT = typename ContainerT::size_type;

   ObjPool(SizeT reserveSize) {
      this->Reserve(reserveSize);
   }
   ContainerT MoveOut() {
      this->FreeIndex_.clear();
      return std::move(this->Objs_);
   }

   /// 如果在 GetPtrObj() 之後有 Add() 或 Remove() 則不保證 retval 持續有效!
   T* GetObjPtr(SizeT idx) {
      if (idx >= this->Objs_.size())
         return nullptr;
      assert(!this->IsInFree(idx));
      return &this->Objs_[idx];
   }
   /// 取得一份 T 的複製.
   T GetObj(SizeT idx) {
      if (idx >= this->Objs_.size())
         return T{};
      assert(!this->IsInFree(idx));
      return this->Objs_[idx];
   }

   /// \return 新加入的 obj 所在的 index.
   template <class X>
   SizeT Add(X obj) {
      SizeT idx;
      if (this->FreeIndex_.empty()) {
         idx = this->Objs_.size();
         this->Objs_.emplace_back(std::move(obj));
      }
      else {
         idx = this->FreeIndex_.back();
         this->FreeIndex_.pop_back();
         this->Objs_[idx] = std::move(obj);
      }
      return idx;
   }

   SizeT Alloc() {
      SizeT idx;
      if (this->FreeIndex_.empty()) {
         idx = this->Objs_.size();
         this->Objs_.resize(idx + 1);
      }
      else {
         idx = this->FreeIndex_.back();
         this->FreeIndex_.pop_back();
      }
      return idx;
   }

   /// 如果 idx 的物件 == obj; 則將 idx 的物件透過 = T{} 的方式清除資源.
   template <class X>
   bool RemoveObj(SizeT idx, X obj) {
      assert(!this->IsInFree(idx));
      if (idx >= this->Objs_.size())
         return false;
      T& vele = this->Objs_[idx];
      if (vele != obj)
         return false;
      vele = T{};
      this->FreeIndex_.push_back(idx);
      return true;
   }

   /// pobj 必須是透過 GetPtrObj(idx) 取得, 且 pobj 必須已經清理過(釋放資源).
   /// 如果 pobj == nullptr, 則不檢查 pobj 是否正確.
   bool RemoveObjPtr(SizeT idx, T* pobj) {
      assert(!this->IsInFree(idx));
      if (idx >= this->Objs_.size())
         return false;
      if (pobj && pobj != &this->Objs_[idx])
         return false;
      this->FreeIndex_.push_back(idx);
      return true;
   }

   void Reserve(SizeT sz) {
      this->Objs_.reserve(sz);
      this->FreeIndex_.reserve(sz);
   }

   SizeT Size() const {
      return this->Objs_.size() - this->FreeIndex_.size();
   }

   bool IsInFree(SizeT idx) const {
      auto ifind = std::find(this->FreeIndex_.begin(), this->FreeIndex_.end(), idx);
      return ifind != this->FreeIndex_.end();
   }

private:
   ContainerT           Objs_;
   std::vector<SizeT>   FreeIndex_;
};

} // namespaces
#endif//__fon9_ObjPool_hpp__

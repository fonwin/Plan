﻿// \file fon9/fmkt/SymbBSData.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbBSData_hpp__
#define __fon9_fmkt_SymbBSData_hpp__
#include "fon9/fmkt/FmktTypes.hpp"
#include "fon9/TimeStamp.hpp"

namespace fon9 { namespace fmkt {

enum class BSFlag : uint8_t {
   /// 試撮後剩餘委託簿.
   Calculated = 0x01,
   /// 委託簿的買方有異動, 或快照有買方資料.
   OrderBuy = 0x02,
   /// 委託簿的賣方有異動, 或快照有賣方資料.
   OrderSell = 0x04,
   /// 委託簿的衍生買方有異動, 或快照有衍生買方資料.
   DerivedBuy = 0x10,
   /// 委託簿的衍生賣方有異動, 或快照有衍生賣方資料.
   DerivedSell = 0x20,
};
fon9_ENABLE_ENUM_BITWISE_OP(BSFlag);

struct SymbBSData {
   enum {
      /// 買賣價量列表數量.
      kBSCount = 5,
   };

   /// 報價時間.
   DayTime  InfoTime_{DayTime::Null()};
   /// 賣出價量列表, [0]=最佳賣出價量.
   PriQty   Sells_[kBSCount];
   /// 買進價量列表, [0]=最佳買進價量.
   PriQty   Buys_[kBSCount];
   /// 衍生賣出.
   PriQty   DerivedSell_;
   /// 衍生買進.
   PriQty   DerivedBuy_;
   BSFlag   Flags_{};
   char     Padding___[7];
};

} } // namespaces
#endif//__fon9_fmkt_SymbBSData_hpp__
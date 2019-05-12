﻿// \file tws/TwsBrk.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_tws_TwsBrk_hpp__
#define __f9omstw_tws_TwsBrk_hpp__
#include "f9omstw/OmsBrk.hpp"

namespace f9omstw {

/// 這裡是一個券商資料的範例, 提供:
/// - 投資人帳號資料表.
/// - 一般而言券商資料, 必定要提供 OrdNoMap, 所以 OrdNoMap 由 OmsBrk 提供.
class TwsBrk : public OmsBrk {
   fon9_NON_COPY_NON_MOVE(TwsBrk);
   using base = OmsBrk;
   struct PodOp;

protected:
   OmsIvacSP MakeIvac(IvacNo) override;

public:
   using base::base;

   static OmsBrkSP BrkMaker(const fon9::StrView& brkid) {
      return new TwsBrk{brkid};
   }

   static fon9::seed::LayoutSP MakeLayout(fon9::seed::TreeFlag flags);
   /// 建立 grid view, 包含 BrkId_; 不含尾端分隔符號.
   void MakeGridRow(fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) override;
   void OnPodOp(OmsBrkTree& brkTree, fon9::seed::FnPodOp&& fnCallback) override;
};
using TwsBrkSP = fon9::intrusive_ptr<TwsBrk>;

} // namespaces
#endif//__f9omstw_tws_TwsBrk_hpp__
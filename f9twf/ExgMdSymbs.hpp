﻿// \file f9twf/ExgMdSymbs.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMdSymbs_hpp__
#define __f9twf_ExgMdSymbs_hpp__
#include "f9twf/Config.h"
#include "fon9/fmkt/SymbTree.hpp"
#include "fon9/fmkt/SymbRef.hpp"
#include "fon9/fmkt/SymbBS.hpp"
#include "fon9/fmkt/SymbDeal.hpp"

namespace f9twf {

class f9twf_API ExgMdSymb : public fon9::fmkt::Symb {
   fon9_NON_COPY_NON_MOVE(ExgMdSymb);
   using base = fon9::fmkt::Symb;
public:
   /// TDayYYYYMMDD_ + TradingSessionId_ 用來判斷資料的新舊.
   /// - 無法使用 I011.END-SESSION 來判斷商品的日夜盤狀態, 因為:
   ///   - 日盤的 I011.END-SESSION 永遠是 '0'一般交易時段;
   ///   - 夜盤的 I011.END-SESSION 永遠是 '1'盤後交易時段;
   /// - 因此必須在收到資料時判斷: 如果已進入夜盤, 則應拋棄日盤的資料.
   /// - 在 DailyClear(tdayYYYYMMDD) 時, 會設定 this->TDayYYYYMMDD_ = tdayYYYYMMDD;
   /// - 在收到夜盤資料時會設定 this->TradingSessionId_ = f9fmkt_TradingSessionId_AfterHour;
   /// - 可使用 this->CheckSetTradingSessionId() 來檢查並設定 TradingSessionId.
   uint32_t             TDayYYYYMMDD_;
   char                 Padding____[4];
   fon9::fmkt::SymbRef  Ref_;
   fon9::fmkt::SymbBS   BS_;
   fon9::fmkt::SymbDeal Deal_;

   using base::base;

   fon9::fmkt::SymbData* GetSymbData(int tabid) override;
   fon9::fmkt::SymbData* FetchSymbData(int tabid) override;

   void DailyClear(unsigned tdayYYYYMMDD);

   /// 檢查並設定 TradingSessionId.
   /// \retval true
   ///   - 現在的 TradingSessionId == tsesId, 表示現在的 TradingSession 正確.
   ///   - 可以設定為新的 tsesId (日盤 => 夜盤).
   ///   - 返回後, 必定 this->TradingSessionId_ == tsesId;
   /// \retval false
   ///   - 現在 symb 所在的盤別較新, 不變動 this->TradingSessionId_;
   ///   - 應拋棄 tsesId 的資料.
   bool CheckSetTradingSessionId(f9fmkt_TradingSessionId tsesId) {
      if (fon9_LIKELY(this->TradingSessionId_ == tsesId))
         return true;
      if (this->TradingSessionId_ == f9fmkt_TradingSessionId_AfterHour)
         return false;
      this->TradingSessionId_ = tsesId;
      return true;
   }

   static fon9::seed::LayoutSP MakeLayout();
};
//--------------------------------------------------------------------------//
class f9twf_API ExgMdSymbs : public fon9::fmkt::SymbTree {
   fon9_NON_COPY_NON_MOVE(ExgMdSymbs);
   using base = fon9::fmkt::SymbTree;
public:
   ExgMdSymbs();

   void DailyClear(unsigned tdayYYYYMMDD);

   fon9::fmkt::SymbSP MakeSymb(const fon9::StrView& symbid) override;
};
using ExgMdSymbsSP = fon9::intrusive_ptr<ExgMdSymbs>;
//--------------------------------------------------------------------------//
struct ExgMdEntry;

/// 返回 mdEntry + mdCount;
f9twf_API const void* ExgMdEntryToSymbBS(fon9::DayTime mdTime, unsigned mdCount, const ExgMdEntry* mdEntry,
                                         fon9::fmkt::SymbBS& symbBS, uint32_t priceOrigDiv);
template <class SymbT>
inline const void* ExgMdEntryToSymbBS(fon9::DayTime mdTime, unsigned mdCount, const ExgMdEntry* mdEntry, SymbT& symb) {
   return ExgMdEntryToSymbBS(mdTime, mdCount, mdEntry, symb.BS_, symb.PriceOrigDiv_);
}
//----------------------
struct ExgMcI081Entry;

f9twf_API void ExgMdEntryToSymbBS(fon9::DayTime mdTime, unsigned mdCount, const ExgMcI081Entry* mdEntry,
                                  fon9::fmkt::SymbBS& symbBS, uint32_t priceOrigDiv);
template <class SymbT>
inline void ExgMdEntryToSymbBS(fon9::DayTime mdTime, unsigned mdCount, const ExgMcI081Entry* mdEntry, SymbT& symb) {
   ExgMdEntryToSymbBS(mdTime, mdCount, mdEntry, symb.BS_, symb.PriceOrigDiv_);
}

} // namespaces
#endif//__f9twf_ExgMdSymbs_hpp__
﻿// \file fon9/seed/SeedVisitor.cpp
// \author fonwinz@gmail.com
#include "fon9/seed/SeedVisitor.hpp"
#include "fon9/buffer/DcQueueList.hpp"
#include "fon9/RevPrint.hpp"

namespace fon9 { namespace seed {

SeedVisitor::~SeedVisitor() {
}
void SeedVisitor::SetCurrPath(StrView currPath) {
   this->Fairy_->SetCurrPath(currPath);
}
//--------------------------------------------------------------------------//
void TicketRunnerError::OnFoundTree(TreeOp&) {
}
void TicketRunnerError::OnLastSeedOp(const PodOpResult&, PodOp*, Tab&) {
}
TicketRunnerSP TicketRunnerError::ArgumentsMustBeEmpty(SeedVisitor& visitor) {
   return new TicketRunnerError(visitor, OpResult::bad_command_argument, "Arguments must be empty.");
}
TicketRunnerSP TicketRunnerError::UnknownCommand(SeedVisitor& visitor) {
   return new TicketRunnerError(visitor, OpResult::not_supported_cmd, "Unknown command.");
}
//--------------------------------------------------------------------------//
void TicketRunnerTree::OnLastStep(TreeOp& opTree, StrView keyText, Tab& tab) {
   this->ContinueSeed(opTree, keyText, tab); // 進入 OnFoundTree();
}
void TicketRunnerTree::OnLastSeedOp(const PodOpResult&, PodOp*, Tab&) {
}
//--------------------------------------------------------------------------//
void TicketRunnerRead::OnFoundTree(TreeOp&) {
   this->OnError(OpResult::not_supported_read);
}
void TicketRunnerRead::OnLastSeedOp(const PodOpResult& resPod, PodOp* pod, Tab& tab) {
   if (pod)
      pod->BeginRead(tab, std::bind(&TicketRunnerRead::OnReadOp,
                                    intrusive_ptr<TicketRunnerRead>(this),
                                    std::placeholders::_1,
                                    std::placeholders::_2));
   else
      this->OnError(resPod.OpResult_);
}
void TicketRunnerRead::OnReadOp(const SeedOpResult& res, const RawRd* rd) {
   if (rd)
      this->Visitor_->OnTicketRunnerRead(*this, res, *rd);
   else
      this->OnError(res.OpResult_);
}
//--------------------------------------------------------------------------//
void TicketRunnerWrite::OnFoundTree(TreeOp&) {
   this->OnError(OpResult::not_supported_write);
}
void TicketRunnerWrite::OnLastStep(TreeOp& op, StrView keyText, Tab& tab) {
   this->RemainPath_.SetBegin(keyText.begin());
   op.Add(keyText, std::bind(&TicketRunnerWrite::OnLastSeedOp,
                             intrusive_ptr<TicketRunnerWrite>(this),
                             std::placeholders::_1,
                             std::placeholders::_2,
                             std::ref(tab)));
}
void TicketRunnerWrite::OnLastSeedOp(const PodOpResult& resPod, PodOp* pod, Tab& tab) {
   if (pod)
      pod->BeginWrite(tab, std::bind(&TicketRunnerWrite::OnWriteOp,
                                     intrusive_ptr<TicketRunnerWrite>(this),
                                     std::placeholders::_1,
                                     std::placeholders::_2));
   else
      this->OnError(resPod.OpResult_);
}
void TicketRunnerWrite::OnWriteOp(const SeedOpResult& res, const RawWr* wr) {
   if (wr)
      this->Visitor_->OnTicketRunnerWrite(*this, res, *wr);
   else
      this->OnError(res.OpResult_);
}
RevBufferList TicketRunnerWrite::ParseSetValues(const SeedOpResult& res, const RawWr& wr) {
   StrView       fldvals{&this->FieldValues_};
   RevBufferList rbuf{128};
   while (!fldvals.empty()) {
      StrView val = SbrFetchNoTrim(fldvals, ',');
      StrView fldName = StrFetchTrim(val, '=');
      auto    fld = res.Tab_->Fields_.Get(fldName);
      if (fld == nullptr)
         RevPrint(rbuf, "fieldName=", fldName, "|err=field not found\n");
      else {
         seed::OpResult r = fld->StrToCell(wr, StrNoTrimRemoveQuotes(val));
         if (r != seed::OpResult::no_error)
            RevPrint(rbuf, "fieldName=", fldName, "|err=StrToCell():", r, ':', seed::GetOpResultMessage(r), '\n');
      }
   }
   return rbuf;
}
//--------------------------------------------------------------------------//
TicketRunnerRemove::TicketRunnerRemove(SeedVisitor& visitor, StrView seed)
   : base(visitor, seed, AccessRight::Write) {
   this->RemovedHandler_ = std::bind(&TicketRunnerRemove::OnRemoved, this, std::placeholders::_1);
}
void TicketRunnerRemove::OnFoundTree(TreeOp&) {
   this->OnError(OpResult::not_supported_remove_pod);
}
void TicketRunnerRemove::OnLastSeedOp(const PodOpResult&, PodOp*, Tab&) {
   // 不會執行到這兒.
   this->OnError(OpResult::not_supported_remove_pod);
}
void TicketRunnerRemove::ContinuePod(TreeOp& opTree, StrView keyText, StrView tabName) {
   this->ContinuePodForRemove(opTree, keyText, tabName, this->RemovedHandler_);
}
void TicketRunnerRemove::OnRemoved(const PodRemoveResult& res) {
   this->OpResult_ = res.OpResult_;
   this->Visitor_->OnTicketRunnerRemoved(*this, res);
}
//--------------------------------------------------------------------------//
TicketRunnerGridView::TicketRunnerGridView(SeedVisitor& visitor, StrView seed, uint16_t reqMaxRowCount, StrView startKey, StrView tabName)
   : base{visitor, seed}
   , StartKeyBuf_{startKey}
   , StartKey_{startKey}
   , TabName_{tabName}
   , ReqMaxRowCount_{reqMaxRowCount} {
   if (!IsTextBeginOrEnd(startKey))
      this->StartKey_ = ToStrView(this->StartKeyBuf_);
}
void TicketRunnerGridView::Continue() {
   this->RemainPath_ = ToStrView(this->OrigPath_);
   this->StartKeyBuf_ = this->LastKey_;
   this->StartKey_ = ToStrView(this->StartKeyBuf_);
}
void TicketRunnerGridView::OnFoundTree(TreeOp& opTree) {
   GridViewRequest req{this->StartKey_};
   req.MaxRowCount_ = this->ReqMaxRowCount_;
   req.Tab_ = opTree.Tree_.LayoutSP_->GetTabByNameOrFirst(ToStrView(this->TabName_));
   if (req.Tab_ == nullptr)
      this->OnError(OpResult::not_found_tab);
   else {
      this->Visitor_->OnTicketRunnerBeforeGridView(*this, opTree, req);
      opTree.GridView(req, std::bind(&TicketRunnerGridView::OnGridViewOp,
                                     intrusive_ptr<TicketRunnerGridView>(this),
                                     std::placeholders::_1));
   }
}
void TicketRunnerGridView::OnGridViewOp(GridViewResult& res) {
   if (res.OpResult_ != OpResult::no_error)
      this->OnError(res.OpResult_);
   else {
      this->LastKey_.assign(res.GetLastKey());
      this->Visitor_->OnTicketRunnerGridView(*this, res);
   }
}
//--------------------------------------------------------------------------//
void TicketRunnerCommand::SetNewCurrPath() {
   this->Visitor_->OnTicketRunnerSetCurrPath(*this);
}
void TicketRunnerCommand::OnFoundTree(TreeOp&) {
   if (this->SeedCommandLine_.empty())
      this->SetNewCurrPath();
   else
      this->OnError(OpResult::not_supported_cmd);
}
void TicketRunnerCommand::OnLastSeedOp(const PodOpResult& resPod, PodOp* pod, Tab& tab) {
   if (!pod)
      this->OnError(resPod.OpResult_);
   else if (this->SeedCommandLine_.empty())
      this->SetNewCurrPath();
   else
      pod->OnSeedCommand(&tab, &this->SeedCommandLine_,
                         std::bind(&TicketRunnerCommand::OnSeedCommandResult,
                                   intrusive_ptr<TicketRunnerCommand>(this),
                                   std::placeholders::_1,
                                   std::placeholders::_2));
}
void TicketRunnerCommand::OnSeedCommandResult(const SeedOpResult& res, StrView msg) {
   this->OpResult_ = res.OpResult_;
   this->Visitor_->OnTicketRunnerCommand(*this, res, msg);
}
//--------------------------------------------------------------------------//
TicketRunnerSubscribe::TicketRunnerSubscribe(SeedVisitor& visitor, StrView seed, StrView tabName)
   : base{visitor, seed}
   , TabName_{tabName}
   , Subr_{visitor.NewSubscribe()} {
}
#define fon9_kCSTR_UNSUB_TAB_NAME   "<u>"
TicketRunnerSubscribe::TicketRunnerSubscribe(SeedVisitor& visitor, StrView seed, VisitorSubr* subr)
   : base{visitor, seed}
   , TabName_{StrView{fon9_kCSTR_UNSUB_TAB_NAME}}
   , Subr_{subr} {
}
void TicketRunnerSubscribe::OnFoundTree(TreeOp& opTree) {
   if (ToStrView(this->TabName_) == fon9_kCSTR_UNSUB_TAB_NAME) {
      assert(this->Subr_);
      if (this->Visitor_->Subr_.Lock()->get() == this->Subr_.get())
         this->Visitor_->Unsubscribe();
      else
         this->Subr_->Unsubscribe();
      this->Visitor_->OnTicketRunnerSubscribe(*this, false);
   }
   else if(Tab* tab = opTree.Tree_.LayoutSP_->GetTabByNameOrFirst(ToStrView(this->TabName_))) {
      OpResult res = this->Subr_->Subscribe(ToStrView(this->OrigPath_), *tab, opTree);
      if (res != OpResult::no_error)
         this->OnError(res);
      else {
         this->Visitor_->OnTicketRunnerSubscribe(*this, true);
         if (this->Visitor_->Subr_.Lock()->get() != this->Subr_.get())
            this->Subr_->Unsubscribe();
      }
   }
   else
      this->OnError(OpResult::not_found_tab);
}

OpResult VisitorSubr::Subscribe(StrView path, Tab& tab, TreeOp& opTree) {
   assert(this->SubConn_ == nullptr && this->Tree_.get() == nullptr);
   if (this->SubConn_ || this->Tree_)
      return OpResult::not_supported_cmd;
   this->Tab_ = &tab;
   this->Path_.assign(path);
   this->Tree_.reset(&opTree.Tree_);
   return opTree.Subscribe(&this->SubConn_, tab,
                           std::bind(&VisitorSubr::OnSeedNotify, VisitorSubrSP{this},
                                     std::placeholders::_1));
}
void VisitorSubr::OnSeedNotify(const SeedNotifyArgs& args) {
   this->Visitor_->OnSeedNotify(*this, args);
}
void VisitorSubr::Unsubscribe() {
   if (auto tree{std::move(this->Tree_)}) {
      SubConn subConn = this->SubConn_;
      this->SubConn_ = nullptr;
      tree->OnTreeOp([subConn](const TreeOpResult&, TreeOp* op) {
         if (op)
            op->Unsubscribe(subConn);
      });
   }
}

void SeedVisitor::Unsubscribe() {
   if (auto subr{std::move(*this->Subr_.Lock())})
      subr->Unsubscribe();
}
VisitorSubrSP SeedVisitor::NewSubscribe() {
   VisitorSubrSP  newSubr{new VisitorSubr{*this}};
   VisitorSubrSP  oldSubr;
   {
      Subr::Locker   locker{this->Subr_};
      oldSubr = std::move(*locker);
      *locker = newSubr;
   }
   if (oldSubr)
      oldSubr->Unsubscribe();
   return newSubr;
}
void SeedVisitor::OnTicketRunnerBeforeGridView(TicketRunnerGridView&, TreeOp&, GridViewRequest&) {
}

} } // namespaces

﻿// \file fon9/seed/FileImpTree.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_seed_FileImpTree_hpp__
#define __fon9_seed_FileImpTree_hpp__
#include "fon9/seed/MaConfigTree.hpp"
#include "fon9/seed/FieldSchCfgStr.hpp"

namespace fon9 { namespace seed {

class fon9_API FileImpTree;

/// 執行載入後解析工作.
/// - 由 Importer 的衍生者複寫 OnBeforeLoad() 返回.
struct fon9_API FileImpLoader : public intrusive_ref_counter<FileImpLoader> {
   unsigned LnCount_{0};

   virtual ~FileImpLoader();

   /// 傳回 pbuf 的剩餘量.
   /// 預設: return this->ParseLine(pbuf, bufsz, isEOF);
   virtual size_t OnLoadBlock(char* pbuf, size_t bufsz, bool isEOF);

   size_t ParseLine(char* pbuf, size_t bufsz, bool isEOF);
   size_t ParseBlock(char* pbuf, size_t bufsz, bool isEOF, size_t lnsz);
   /// 使用 ParseLine() 或 ParseBlock() 時, 每一行到此處理.
   virtual void OnLoadLine(char* pbuf, size_t bufsz, bool isEOF);
};
using FileImpLoaderSP = intrusive_ptr<FileImpLoader>;

enum class FileImpMonitorFlag : char {
   None = 0,
   /// 匯入檔有異動時, 全部重新載入.
   Reload = 'R',
   /// 匯入檔大小有異動時, 載入尾端新增的部分.
   AddTail = 'A',
};

fon9_WARN_DISABLE_PADDING;
/// 一筆匯入資料的設定、狀態、載入.
/// - Fields:
///   - FieldName(Title) = Import file name;
///   - Mon(Value) = Auto reload when FileTime changed.
///   - Sch
///   - Result(Description) = Import result;
/// - SeedCommand: "reload" 可手動載入.
/// - 衍生者須完成:
///   - `FileImpLoaderSP OnBeforeLoad(uint64_t fileSize) override;`
///   - `void OnAfterLoad(RevBuffer& rbuf, FileImpLoaderSP loader) override;`
class fon9_API FileImpSeed : public MaConfigSeed {
   fon9_NON_COPY_NON_MOVE(FileImpSeed);
   using base = MaConfigSeed;

   bool           IsReloading_{false};
   bool           IsForceLoadOnce_{false};
   SchCfgStr      SchCfgStr_;
   TimeStamp      LastFileTime_;
   /// 檔案異動使用「增量載入」時, 記錄上次載入的位置, 及尚未處理的不完整訊息.
   File::PosType  LastPos_{};
   std::string    LastRemain_;

   /// ulk 處在 unlock 狀態, 如果需要更新 this->SetDescription() 之類, 要先 ulk.lock();
   void OnSeedCommand(SeedOpResult&          res,
                      StrView                cmdln,
                      FnCommandResultHandler resHandler,
                      ConfigLocker&&         ulk) override;
   void OnConfigValueChanged(MaConfigTree::Locker&& lk, StrView val) override;

   /// 如果不支援 FileImpMonitorFlag::AddTail;
   /// - 傳回 nullptr;
   /// - 或呼叫 this->ClearAddTailRemain(); 之後返回「支援重新載入」的處理程序.
   ///   返回前必要時, 可修改 monFlag;
   virtual FileImpLoaderSP OnBeforeLoad(uint64_t fileSize, FileImpMonitorFlag& monFlag) = 0;
   virtual void OnAfterLoad(RevBuffer& rbuf, FileImpLoaderSP loader) = 0;

protected:
   virtual void Reload(ConfigLocker&& lk, std::string fname, FileImpMonitorFlag monFlag);

   /// ForceLoadOnce == true 表示暫時不考慮 Sch 的設定, 啟動計時器, 並強制載入一次.
   /// - 預設為 false; 衍生者可在建構時(或其他適當時機), 將此旗標設為 true: 強制重載.
   /// - 在每次 Reload() 時, 會清除此旗標: ForceLoadOnce = false;
   void SetForceLoadOnce(ConfigLocker&& lk, bool v);

   void ClearAddTailRemain() {
      this->LastPos_ = 0;
      this->LastRemain_.clear();
   }

   /// 通常在衍生者建構時提供預設值.
   void SetSchCfgStr(const StrView& cfgstr) {
      this->SchCfgStr_.AssignCfgStr(cfgstr);
   }

public:
   template <class... ArgsT>
   FileImpSeed(FileImpTree& owner, ArgsT&&... args)
      : base(owner, std::forward<ArgsT>(args)...) {
   }

   ~FileImpSeed();

   FileImpMonitorFlag GetMonitorFlag(const ConfigLocker& lk) const {
      auto cfg = this->GetConfigValue(lk);
      return cfg.empty() ? FileImpMonitorFlag::None : static_cast<FileImpMonitorFlag>(*cfg.begin());
   }
   bool IsForceLoadOnce() const {
      return this->IsForceLoadOnce_;
   }

   /// 返回: 多久後再次檢查(建議值), TimeInterval{} 表示不用檢查.
   TimeInterval MonitorCheck(ConfigLocker&& lk, TimeStamp now);

   TimeStamp LastFileTime() const {
      return this->LastFileTime_;
   }
   /// 清除 AddTail 已載入的資料, 並使用排程機制重新載入.
   /// 預設不會改變 lk 的鎖定狀態.
   virtual void ClearReload(ConfigLocker&& lk);

   /// 載入完畢(或失敗)後才會返回.
   /// - 返回時 ulk 必定在 lock() 狀態.
   /// - 返回時 this->GetDescription() 顯示結果.
   /// - 如果重複進入, 則後進者會立即返回, this->GetDescription() 顯示 "Loading";
   /// - 若 fname 與現在的(Title)不同, 則會更新 Title, 並儲存設定.
   void LoadFrom(ConfigLocker&& ulk, StrView fname);

   static LayoutSP MakeLayout(TabFlag tabFlag = TabFlag::NoSapling | TabFlag::Writable);
};
using FileImpSeedSP = intrusive_ptr<FileImpSeed>;
fon9_WARN_POP;

class fon9_API FileImpTree : public MaConfigTree {
   fon9_NON_COPY_NON_MOVE(FileImpTree);
   using base = MaConfigTree;
public:
   FileImpTree(MaConfigMgrBase& mgr, LayoutSP layout = FileImpSeed::MakeLayout())
      : base{mgr, std::move(layout)} {
   }

   /// 全部載入完畢後才返回.
   void LoadAll();
};
using FileImpTreeSP = intrusive_ptr<FileImpTree>;

/// 管理一組匯入資料的設定、狀態、載入.
/// - SeedCommand: "reload" 全部載入.
/// - 排程時間到, 會切換到 DefaultThreadPool 載入, 避免佔用 DefaultTimerThread 的執行權.
/// - 透過 MaConfigSeed::SetConfigValue() 設定排程參數.
class fon9_API FileImpMgr : public MaConfigSeed_SchTask, public MaConfigMgrBase {
   fon9_NON_COPY_NON_MOVE(FileImpMgr);
   using base = MaConfigSeed_SchTask;

   static void OnMonitorTimer(TimerEntry* timer, TimeStamp now);
   DataMemberEmitOnTimer<&FileImpMgr::OnMonitorTimer> MonitorTimer_;

protected:
   void OnSeedCommand(SeedOpResult&          res,
                      StrView                cmdln,
                      FnCommandResultHandler resHandler,
                      ConfigLocker&&         ulk) override;
   void OnSchCfgChanged(StrView cfgstr) override;
   void OnSchTask_StateChanged(bool isInSch) override;

public:
   template <class... ArgsT>
   FileImpMgr(FileImpTreeSP sapling, ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...)
      , MaConfigMgrBase{std::move(sapling)} {
   }

   ~FileImpMgr();

   FileImpTree& GetFileImpSapling() const {
      assert(dynamic_cast<FileImpTree*>(this->Sapling_.get()) != nullptr);
      return *static_cast<FileImpTree*>(this->Sapling_.get());
   }

   TreeSP GetSapling() override;
   StrView Name() const override;

   void BindConfigFile(std::string cfgfn, bool isFireEvent) {
      MaConfigMgrBase::BindConfigFile(*this, *this, std::move(cfgfn), isFireEvent);
   }

   /// 全部載入完畢後才返回.
   void LoadAll();

   /// 清除 AddTail 已載入的資料, 並使用排程機制重新載入.
   void ClearReloadAll();

   void CheckStartMonitor(FileImpTree::Locker&&);
};

} } // namespaces
#endif//__fon9_seed_FileImpTree_hpp__

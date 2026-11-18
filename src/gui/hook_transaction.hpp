#pragma once

namespace gui
{
    // Own only the steps completed by this installation attempt.
    template<class Operations> class hook_transaction
    {
        Operations& operations_;
        bool engine_=false, hook_=false, committed_=false;
    public:
        explicit hook_transaction(Operations& operations) : operations_(operations) {}
        hook_transaction(const hook_transaction&)=delete;
        bool initialize() { return engine_=operations_.initialize(); }
        bool create() { return engine_ && (hook_=operations_.create()); }
        bool enable() { return hook_ && operations_.enable(); }
        void commit() { committed_=true; }
        ~hook_transaction()
        {
            if (committed_) return;
            if (hook_) operations_.remove();
            if (engine_) operations_.uninitialize();
        }
    };
}

#include "SessionController.h"
#include "CoreEngine.h"
#include "ISessionService.h"

// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include <cppmicroservices/BundleActivator.h>

#include <memory>

namespace recplay {

class SessionActivator : public cppmicroservices::BundleActivator {
public:
    void Start(cppmicroservices::BundleContext context) override {
        auto core_engine_reference = context.GetServiceReference<CoreEngine>();
        if (core_engine_reference) {
            core_engine_ = context.GetService(core_engine_reference);
        }

        controller_ = std::make_shared<SessionController>(core_engine_.get());
        context.RegisterService<ISessionService>(controller_);
    }

    void Stop(cppmicroservices::BundleContext context) override {
        (void)context;
        if (controller_ != nullptr) {
            switch (controller_->GetState()) {
                case SessionState::Recording:
                case SessionState::RecordingPaused:
                    controller_->StopRecording();
                    break;
                case SessionState::Playing:
                case SessionState::PlaybackPaused:
                case SessionState::Seeking:
                    controller_->Stop();
                    break;
                case SessionState::Idle:
                case SessionState::Stopped:
                    break;
            }
        }
        controller_.reset();
        core_engine_.reset();
    }

private:
    std::shared_ptr<CoreEngine> core_engine_;
    std::shared_ptr<SessionController> controller_;
};

} // namespace recplay

CPPMICROSERVICES_EXPORT_BUNDLE_ACTIVATOR(recplay::SessionActivator)

/*
    SPDX-FileCopyrightText: 2023 jccKevin <luochaojiang@uniontech.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "display.h"
#include "outputdevice_v1_interface.h"
#include "outputmanagement_v1_interface.h"
#include "utils/common.h"

#include "core/outputbackend.h"
#include "core/outputconfiguration.h"
#include "main.h"
#include "workspace.h"

#include "qwayland-server-org-kde-kwin-outputdevice.h"
#include "qwayland-server-outputmanagement.h"

#include <optional>

using namespace KWin;

namespace KWaylandServer
{

static const quint32 s_version = 4;

class OutputManagementInterfacePrivate : public QtWaylandServer::org_kde_kwin_outputmanagement
{
public:
    OutputManagementInterfacePrivate(Display *display);

protected:
    void org_kde_kwin_outputmanagement_create_configuration(Resource *resource, uint32_t id) override;
};

class OutputConfigurationInterface : public QObject, QtWaylandServer::org_kde_kwin_outputconfiguration
{
    Q_OBJECT
public:
    explicit OutputConfigurationInterface(wl_resource *resource);

    bool applied = false;
    bool invalid = false;
    OutputConfiguration config;
    QVector<std::pair<uint32_t, OutputDeviceInterface *>> outputOrder;

protected:
    void org_kde_kwin_outputconfiguration_enable(Resource *resource, wl_resource *outputdevice, int32_t enable) override;
    void org_kde_kwin_outputconfiguration_mode(Resource *resource, wl_resource *outputdevice, int32_t mode_id) override;
    void org_kde_kwin_outputconfiguration_transform(Resource *resource, wl_resource *outputdevice, int32_t transform) override;
    void org_kde_kwin_outputconfiguration_position(Resource *resource, wl_resource *outputdevice, int32_t x, int32_t y) override;
    void org_kde_kwin_outputconfiguration_scale(Resource *resource, wl_resource *outputdevice, int32_t scale) override;
    void org_kde_kwin_outputconfiguration_apply(Resource *resource) override;
    void org_kde_kwin_outputconfiguration_scalef(Resource *resource, wl_resource *outputdevice, wl_fixed_t scale) override;
    void org_kde_kwin_outputconfiguration_colorcurves(Resource *resource, wl_resource *outputdevice, wl_array *red, wl_array *green, wl_array *blue) override;
    void org_kde_kwin_outputconfiguration_destroy(Resource *resource) override;
    void org_kde_kwin_outputconfiguration_destroy_resource(Resource *resource) override;
    void org_kde_kwin_outputconfiguration_overscan(Resource *resource, wl_resource *outputdevice, uint32_t overscan) override;
    void org_kde_kwin_outputconfiguration_set_vrr_policy(Resource *resource, wl_resource *outputdevice, uint32_t policy) override;
    void org_kde_kwin_outputconfiguration_brightness(Resource *resource, wl_resource *outputdevice, int32_t brightness) override;
    void org_kde_kwin_outputconfiguration_ctm(Resource *resource, wl_resource *outputdevice, int32_t red, int32_t green, int32_t blue) override;
};

OutputManagementInterfacePrivate::OutputManagementInterfacePrivate(Display *display)
    : QtWaylandServer::org_kde_kwin_outputmanagement(*display, s_version)
{
}

void OutputManagementInterfacePrivate::org_kde_kwin_outputmanagement_create_configuration(Resource *resource, uint32_t id)
{
    qCDebug(KWIN_CORE) << "outputv1:resource " << resource << " id " << id;
    wl_resource *config_resource = wl_resource_create(resource->client(), &org_kde_kwin_outputconfiguration_interface, resource->version(), id);
    if (!config_resource) {
        qCDebug(KWIN_CORE) << "outputv1:resource " << resource << " wl_client_post_no_memory";
        wl_client_post_no_memory(resource->client());
        return;
    }
    new OutputConfigurationInterface(config_resource);
}

OutputManagementInterface::OutputManagementInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new OutputManagementInterfacePrivate(display))
{
}

OutputManagementInterface::~OutputManagementInterface() = default;

OutputConfigurationInterface::OutputConfigurationInterface(wl_resource *resource)
    : QtWaylandServer::org_kde_kwin_outputconfiguration(resource)
{
    const auto reject = [this](Output *output) {
        invalid = true;
    };
    // luocj
    connect(workspace(), &Workspace::outputAdded, this, reject);
    connect(workspace(), &Workspace::outputRemoved, this, reject);
}

void OutputConfigurationInterface::org_kde_kwin_outputconfiguration_enable(Resource *resource, wl_resource *outputdevice, int32_t enable)
{
    qCDebug(KWIN_CORE) << "outputv1:outputdevice " << outputdevice << " invalid " << invalid << " enable " << enable;
    if (invalid) {
        return;
    }
    if (OutputDeviceInterface *output = OutputDeviceInterface::get(outputdevice)) {
        config.changeSet(output->handle())->enabled = enable;
    }
}

void OutputConfigurationInterface::org_kde_kwin_outputconfiguration_mode(Resource *resource, wl_resource *outputdevice, int32_t mode_id)
{
    qCDebug(KWIN_CORE) << "outputv1:outputdevice " << outputdevice << " invalid " << invalid << " mode_id " << mode_id;
    if (invalid) {
        return;
    }
    OutputDeviceInterface *output = OutputDeviceInterface::get(outputdevice);
    OutputDeviceModeInterface *mode = output->getMode(mode_id);
    if (output && mode) {
        config.changeSet(output->handle())->mode = mode->m_handle.lock();
    } else {
        invalid = true;
    }
}

void OutputConfigurationInterface::org_kde_kwin_outputconfiguration_transform(Resource *resource, wl_resource *outputdevice, int32_t transform)
{
    qCDebug(KWIN_CORE) << "outputv1:outputdevice " << outputdevice << " invalid " << invalid << " enable " << transform;
    if (invalid) {
        return;
    }
    auto toTransform = [transform]() {
        switch (transform) {
        case WL_OUTPUT_TRANSFORM_90:
            return Output::Transform::Rotated90;
        case WL_OUTPUT_TRANSFORM_180:
            return Output::Transform::Rotated180;
        case WL_OUTPUT_TRANSFORM_270:
            return Output::Transform::Rotated270;
        case WL_OUTPUT_TRANSFORM_FLIPPED:
            return Output::Transform::Flipped;
        case WL_OUTPUT_TRANSFORM_FLIPPED_90:
            return Output::Transform::Flipped90;
        case WL_OUTPUT_TRANSFORM_FLIPPED_180:
            return Output::Transform::Flipped180;
        case WL_OUTPUT_TRANSFORM_FLIPPED_270:
            return Output::Transform::Flipped270;
        case WL_OUTPUT_TRANSFORM_NORMAL:
        default:
            return Output::Transform::Normal;
        }
    };
    auto _transform = toTransform();
    if (OutputDeviceInterface *output = OutputDeviceInterface::get(outputdevice)) {
        config.changeSet(output->handle())->transform = _transform;
    }
}

void OutputConfigurationInterface::org_kde_kwin_outputconfiguration_position(Resource *resource, wl_resource *outputdevice, int32_t x, int32_t y)
{
    qCDebug(KWIN_CORE) << "outputv1:outputdevice " << outputdevice << " invalid " << invalid << " x " << x << " y " << y;
    if (invalid) {
        return;
    }
    if (OutputDeviceInterface *output = OutputDeviceInterface::get(outputdevice)) {
        config.changeSet(output->handle())->pos = QPoint(x, y);
    }
}

void OutputConfigurationInterface::org_kde_kwin_outputconfiguration_scale(Resource *resource, wl_resource *outputdevice, int32_t scale)
{
    qCDebug(KWIN_CORE) << "outputv1:outputdevice " << outputdevice << " invalid " << invalid << " scale " << scale;
    if (invalid) {
        return;
    }
    qreal doubleScale = wl_fixed_to_double(scale);

    // the fractional scaling protocol only speaks in unit of 120ths
    // using the same scale throughout makes that simpler
    // this also eliminates most loss from wl_fixed
    doubleScale = std::round(doubleScale * 120) / 120;

    if (doubleScale <= 0) {
        qCWarning(KWIN_CORE) << "Requested to scale output device to" << doubleScale << ", but I can't do that.";
        return;
    }

    if (OutputDeviceInterface *output = OutputDeviceInterface::get(outputdevice)) {
        config.changeSet(output->handle())->scale = doubleScale;
    }
}

void OutputConfigurationInterface::org_kde_kwin_outputconfiguration_scalef(Resource *resource, wl_resource *outputdevice, wl_fixed_t scale)
{
    qCDebug(KWIN_CORE) << "outputv1:outputdevice " << outputdevice << " invalid " << invalid << " scale " << scale;
    if (invalid) {
        return;
    }
    qreal doubleScale = wl_fixed_to_double(scale);

    // the fractional scaling protocol only speaks in unit of 120ths
    // using the same scale throughout makes that simpler
    // this also eliminates most loss from wl_fixed
    doubleScale = std::round(doubleScale * 120) / 120;

    if (doubleScale <= 0) {
        qCWarning(KWIN_CORE) << "Requested to scale output device to" << doubleScale << ", but I can't do that.";
        return;
    }

    if (OutputDeviceInterface *output = OutputDeviceInterface::get(outputdevice)) {
        config.changeSet(output->handle())->scale = doubleScale;
    }
}

void OutputConfigurationInterface::org_kde_kwin_outputconfiguration_overscan(Resource *resource, wl_resource *outputdevice, uint32_t overscan)
{
    qCDebug(KWIN_CORE) << "outputv1:outputdevice " << outputdevice << " invalid " << invalid << " overscan " << overscan;
    if (invalid) {
        return;
    }
    if (overscan > 100) {
        qCWarning(KWIN_CORE) << "Invalid overscan requested:" << overscan;
        return;
    }
    if (OutputDeviceInterface *output = OutputDeviceInterface::get(outputdevice)) {
        config.changeSet(output->handle())->overscan = overscan;
    }
}

void OutputConfigurationInterface::org_kde_kwin_outputconfiguration_set_vrr_policy(Resource *resource, wl_resource *outputdevice, uint32_t policy)
{
    qCDebug(KWIN_CORE) << "outputv1:outputdevice " << outputdevice << " invalid " << invalid << " policy " << policy;
    if (invalid) {
        return;
    }
    if (policy > static_cast<uint32_t>(RenderLoop::VrrPolicy::Automatic)) {
        qCWarning(KWIN_CORE) << "Invalid Vrr Policy requested:" << policy;
        return;
    }
    if (OutputDeviceInterface *output = OutputDeviceInterface::get(outputdevice)) {
        config.changeSet(output->handle())->vrrPolicy = static_cast<RenderLoop::VrrPolicy>(policy);
    }
}

void OutputConfigurationInterface::org_kde_kwin_outputconfiguration_colorcurves(Resource *resource, wl_resource *outputdevice, wl_array *red, wl_array *green, wl_array *blue)
{
    qCDebug(KWIN_CORE) << "outputv1:outputdevice " << outputdevice << " invalid " << invalid << " red " << red;
    //luocj
}

void OutputConfigurationInterface::org_kde_kwin_outputconfiguration_brightness(Resource *resource, wl_resource *outputdevice, int32_t brightness)
{
    qCDebug(KWIN_CORE) << "outputv1:outputdevice " << outputdevice << " invalid " << invalid << " brightness " << brightness;
    if (invalid) {
        return;
    }
    if (OutputDeviceInterface *output = OutputDeviceInterface::get(outputdevice)) {
        config.changeSet(output->handle())->brightness = brightness;
    }
}

void OutputConfigurationInterface::org_kde_kwin_outputconfiguration_ctm(Resource *resource, wl_resource *outputdevice, int32_t r, int32_t g, int32_t b)
{
    qCDebug(KWIN_CORE) << "outputv1:outputdevice " << outputdevice << " invalid " << invalid << " r " << r;
    if (invalid) {
        return;
    }
    if (OutputDeviceInterface *output = OutputDeviceInterface::get(outputdevice)) {
        config.changeSet(output->handle())->ctmValue = Output::CtmValue{(uint16_t)r, (uint16_t)g, (uint16_t)b};;
    }
}

void OutputConfigurationInterface::org_kde_kwin_outputconfiguration_destroy(Resource *resource)
{
    qCDebug(KWIN_CORE) << "outputv1:resource " << resource << " invalid " << invalid;
    wl_resource_destroy(resource->handle);
}

void OutputConfigurationInterface::org_kde_kwin_outputconfiguration_destroy_resource(Resource *resource)
{
    qCDebug(KWIN_CORE) << "outputv1:resource " << resource << " invalid " << invalid;
    delete this;
}

void OutputConfigurationInterface::org_kde_kwin_outputconfiguration_apply(Resource *resource)
{
    qCDebug(KWIN_CORE) << "outputv1:resource " << resource << " invalid " << invalid;
    if (applied) {
        qCWarning(KWIN_CORE) << "Rejecting because an output configuration can be applied only once";
        return;
    }

    applied = true;
    if (invalid) {
        qCWarning(KWIN_CORE) << "Rejecting configuration change because a request output is no longer available";
        applied = false;
        send_failed();
        return;
    }

    const auto allOutputs = kwinApp()->outputBackend()->outputs();
    const bool allDisabled = !std::any_of(allOutputs.begin(), allOutputs.end(), [this](const auto &output) {
        return config.constChangeSet(output)->enabled;
    });
    if (allDisabled) {
        qCWarning(KWIN_CORE) << "Disabling all outputs through configuration changes is not allowed";
        applied = false;
        send_failed();
        return;
    }

    QVector<Output *> sortedOrder;
    if (!outputOrder.empty()) {
        const int desktopOutputs = std::count_if(allOutputs.begin(), allOutputs.end(), [](Output *output) {
            qCDebug(KWIN_CORE) << "outputv1:resource " << " isNonDesktop ";
            return !output->isNonDesktop();
        });
        if (outputOrder.size() != desktopOutputs) {
            qWarning(KWIN_CORE) << "Provided output order doesn't contain all outputs!";
            applied = false;
            send_failed();
            return;
        }
        outputOrder.erase(std::remove_if(outputOrder.begin(), outputOrder.end(), [this](const auto &pair) {
                              return !config.constChangeSet(pair.second->handle())->enabled;
                          }),
                          outputOrder.end());
        std::sort(outputOrder.begin(), outputOrder.end(), [](const auto &pair1, const auto &pair2) {
            qCDebug(KWIN_CORE) << "outputv1:resource " << " pair2 ";
            return pair1.first < pair2.first;
        });
        uint32_t i = 1;
        for (const auto &[index, name] : std::as_const(outputOrder)) {
            if (index != i) {
                qCWarning(KWIN_CORE) << "Provided output order is invalid!";
                applied = false;
                send_failed();
                return;
            }
            i++;
        }
        sortedOrder.reserve(outputOrder.size());
        std::transform(outputOrder.begin(), outputOrder.end(), std::back_inserter(sortedOrder), [](const auto &pair) {
            qCDebug(KWIN_CORE) << "outputv1:resource " << " second handle";
            return pair.second->handle();
        });
    }
    if (workspace()->applyOutputConfiguration(config, sortedOrder)) {
        qCDebug(KWIN_CORE) << "outputv1:resource " << resource << " send_applied";
        applied = false;
        send_applied();
    } else {
        qCDebug(KWIN_CORE) << "Applying config failed";
        applied = false;
        send_failed();
    }
}

}

#include "outputmanagement_v1_interface.moc"
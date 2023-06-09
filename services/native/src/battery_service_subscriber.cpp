/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "battery_service_subscriber.h"

#include <typeinfo>
#include "ohos/aafwk/content/want.h"
#include "battery_log.h"
#include "string_ex.h"
#include "batteryd_api.h"
#include "iservice_registry.h"
#include "if_system_ability_manager.h"
#include "system_ability_definition.h"
#include "hisysevent.h"

using namespace OHOS::AAFwk;
using namespace OHOS::EventFwk;
using namespace OHOS::HDI::Battery;
using namespace OHOS::HiviewDFX;

namespace OHOS {
namespace PowerMgr {
bool g_firstPublish = true;
bool g_batteryLowOnce = false;
bool g_batteryOkOnce = false;
bool g_batteryConnectOnce = false;
bool g_batteryDisconnectOnce = false;
bool g_batteryChargingOnce = false;
bool g_batteryDischargingOnce = false;
bool g_commonEventInitSuccess = false;
BatterydInfo g_batteryInfo;
const int BATTERY_LOW_CAPACITY = 20;

int32_t BatteryServiceSubscriber::Update(const BatteryInfo& info)
{
    if (g_commonEventInitSuccess) {
        BATTERY_HILOGI(COMP_SVC, "common event service ability init success");
    } else {
        if (!IsCommonEventServiceAbilityExist()) {
            return ERR_NO_INIT;
        }
    }

    bool isAllSuccess = true;
    bool ret = HandleBatteryChangedEvent(info);
    isAllSuccess &= ret;
    ret = HandleBatteryLowEvent(info);
    isAllSuccess &= ret;
    ret = HandleBatteryOkayEvent(info);
    isAllSuccess &= ret;
    ret = HandleBatteryPowerConnectedEvent(info);
    isAllSuccess &= ret;
    ret = HandleBatteryPowerDisconnectedEvent(info);
    isAllSuccess &= ret;
    ret = HandleBatteryChargingEvent(info);
    isAllSuccess &= ret;
    ret = HandleBatteryDischargingEvent(info);
    isAllSuccess &= ret;
    g_firstPublish = false;

    return isAllSuccess ? ERR_OK : ERR_NO_INIT;
}

bool BatteryServiceSubscriber::IsCommonEventServiceAbilityExist()
{
    sptr<ISystemAbilityManager> sysMgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (!sysMgr) {
        BATTERY_HILOGI(COMP_SVC,
            "IsCommonEventServiceAbilityExist Get ISystemAbilityManager failed, no SystemAbilityManager");
        return false;
    }
    sptr<IRemoteObject> remote = sysMgr->CheckSystemAbility(COMMON_EVENT_SERVICE_ID);
    if (!remote) {
        BATTERY_HILOGE(COMP_SVC, "No CesServiceAbility");
        return false;
    }

    g_commonEventInitSuccess = true;
    return true;
}

bool BatteryServiceSubscriber::HandleBatteryChangedEvent(const BatteryInfo& info)
{
    BATTERY_HILOGD(FEATURE_BATT_INFO, "enter");
    Want want;
    want.SetParam(ToString(BatteryInfo::COMMON_EVENT_CODE_CAPACITY), info.GetCapacity());
    want.SetParam(ToString(BatteryInfo::COMMON_EVENT_CODE_VOLTAGE), info.GetVoltage());
    want.SetParam(ToString(BatteryInfo::COMMON_EVENT_CODE_TEMPERATURE), info.GetTemperature());
    want.SetParam(ToString(BatteryInfo::COMMON_EVENT_CODE_HEALTH_STATE), static_cast<int>(info.GetHealthState()));
    want.SetParam(ToString(BatteryInfo::COMMON_EVENT_CODE_PLUGGED_TYPE), static_cast<int>(info.GetPluggedType()));
    want.SetParam(ToString(BatteryInfo::COMMON_EVENT_CODE_PLUGGED_MAX_CURRENT), info.GetPluggedMaxCurrent());
    want.SetParam(ToString(BatteryInfo::COMMON_EVENT_CODE_PLUGGED_MAX_VOLTAGE), info.GetPluggedMaxVoltage());
    want.SetParam(ToString(BatteryInfo::COMMON_EVENT_CODE_CHARGE_STATE), static_cast<int>(info.GetChargeState()));
    want.SetParam(ToString(BatteryInfo::COMMON_EVENT_CODE_CHARGE_COUNTER), info.GetChargeCounter());
    want.SetParam(ToString(BatteryInfo::COMMON_EVENT_CODE_PRESENT), info.IsPresent());
    want.SetParam(ToString(BatteryInfo::COMMON_EVENT_CODE_TECHNOLOGY), info.GetTechnology());
    want.SetParam(ToString(BatteryInfo::COMMON_EVENT_CODE_PLUGGED_NOW_CURRENT), info.GetNowCurrent());

    want.SetAction(CommonEventSupport::COMMON_EVENT_BATTERY_CHANGED);
    CommonEventData data;
    data.SetWant(want);
    CommonEventPublishInfo publishInfo;
    publishInfo.SetOrdered(false);
    bool isSuccess = true;

    if ((g_firstPublish == true) || (CmpBatteryInfo(info) == false)) {
        HiSysEvent::Write(HiSysEvent::Domain::POWERMGR, "BATTERY_CHANGED", HiSysEvent::EventType::STATISTIC,
            "LEVEL", info.GetCapacity(), "CHARGER", static_cast<int>(info.GetPluggedType()),
            "VOLTAGE", info.GetVoltage(), "TEMP", info.GetTemperature(),
            "HEALTH", static_cast<int>(info.GetHealthState()), "CURRENT", info.GetNowCurrent());
        isSuccess = CommonEventManager::PublishCommonEvent(data, publishInfo);
        SwaptBatteryInfo(info);
    }

    if (!isSuccess) {
        BATTERY_HILOGD(FEATURE_BATT_INFO, "failed to publish CAPACITY_CHANGED event");
    }
    return isSuccess;
}

bool BatteryServiceSubscriber::CmpBatteryInfo(const BatteryInfo& info)
{
    BATTERY_HILOGD(FEATURE_BATT_INFO, "enter");
    return ((g_batteryInfo.capacity_ == info.GetCapacity()) &&
            (g_batteryInfo.voltage_ == info.GetVoltage()) &&
            (g_batteryInfo.temperature_ == info.GetTemperature()) &&
            (g_batteryInfo.healthState_ == static_cast<int32_t>(info.GetHealthState())) &&
            (g_batteryInfo.pluggedType_ == static_cast<int32_t>(info.GetPluggedType())) &&
            (g_batteryInfo.pluggedMaxCurrent_ == info.GetPluggedMaxCurrent()) &&
            (g_batteryInfo.pluggedMaxVoltage_ == info.GetPluggedMaxVoltage()) &&
            (g_batteryInfo.chargeState_ == static_cast<int32_t>(info.GetChargeState())) &&
            (g_batteryInfo.chargeCounter_ == info.GetChargeCounter()) &&
            (g_batteryInfo.present_ == info.IsPresent()) &&
            (g_batteryInfo.curNow_) == info.GetNowCurrent());
}

void BatteryServiceSubscriber::SwaptBatteryInfo(const BatteryInfo& info)
{
    BATTERY_HILOGD(FEATURE_BATT_INFO, "enter");
    g_batteryInfo.capacity_ = info.GetCapacity();
    g_batteryInfo.voltage_ = info.GetVoltage();
    g_batteryInfo.temperature_ = info.GetTemperature();
    g_batteryInfo.healthState_ = static_cast<uint32_t>(info.GetHealthState());
    g_batteryInfo.pluggedType_ = static_cast<uint32_t>(info.GetPluggedType());
    g_batteryInfo.pluggedMaxCurrent_ = info.GetPluggedMaxCurrent();
    g_batteryInfo.pluggedMaxVoltage_ = info.GetPluggedMaxVoltage();
    g_batteryInfo.chargeState_ = static_cast<int32_t>(info.GetChargeState());
    g_batteryInfo.chargeCounter_ = info.GetChargeCounter();
    g_batteryInfo.present_ = info.IsPresent();
    g_batteryInfo.curNow_ = info.GetNowCurrent();
}

bool BatteryServiceSubscriber::HandleBatteryLowEvent(const BatteryInfo& info)
{
    Want want;
    want.SetAction(CommonEventSupport::COMMON_EVENT_BATTERY_LOW);
    CommonEventData data;
    data.SetWant(want);
    CommonEventPublishInfo publishInfo;
    publishInfo.SetOrdered(false);
    bool isSuccess = true;

    if (info.GetCapacity() > BATTERY_LOW_CAPACITY) {
        g_batteryLowOnce = false;
        return isSuccess;
    }

    if (g_batteryLowOnce) {
        return isSuccess;
    }

    data.SetCode(BatteryInfo::COMMON_EVENT_CODE_CAPACITY);
    data.SetData(ToString(info.GetCapacity()));
    BATTERY_HILOGD(FEATURE_BATT_INFO, "publisher capacity=%{public}d", info.GetCapacity());
    isSuccess = CommonEventManager::PublishCommonEvent(data, publishInfo);
    if (!isSuccess) {
        BATTERY_HILOGD(FEATURE_BATT_INFO, "failed to publish battery_low event");
    }
    g_batteryLowOnce = true;
    return isSuccess;
}

bool BatteryServiceSubscriber::HandleBatteryOkayEvent(const BatteryInfo& info)
{
    Want want;
    want.SetAction(CommonEventSupport::COMMON_EVENT_BATTERY_OKAY);
    CommonEventData data;
    data.SetWant(want);
    CommonEventPublishInfo publishInfo;
    publishInfo.SetOrdered(false);
    bool isSuccess = true;

    if (info.GetCapacity() <= BATTERY_LOW_CAPACITY) {
        g_batteryOkOnce = false;
        return isSuccess;
    }

    if (g_batteryOkOnce) {
        return isSuccess;
    }

    data.SetCode(BatteryInfo::COMMON_EVENT_CODE_CAPACITY);
    data.SetData(ToString(info.GetCapacity()));
    BATTERY_HILOGD(FEATURE_BATT_INFO, "publisher capacity=%{public}d", info.GetCapacity());
    isSuccess = CommonEventManager::PublishCommonEvent(data, publishInfo);
    if (!isSuccess) {
        BATTERY_HILOGD(FEATURE_BATT_INFO, "failed to publish battery_okay event");
    }
    g_batteryOkOnce = true;
    return isSuccess;
}

bool BatteryServiceSubscriber::HandleBatteryPowerConnectedEvent(const BatteryInfo& info)
{
    Want want;
    want.SetAction(CommonEventSupport::COMMON_EVENT_POWER_CONNECTED);
    CommonEventData data;
    data.SetWant(want);
    CommonEventPublishInfo publishInfo;
    publishInfo.SetOrdered(false);
    bool isSuccess = true;

    if ((static_cast<uint32_t>(info.GetPluggedType()) == PLUGGED_TYPE_NONE) ||
        (static_cast<uint32_t>(info.GetPluggedType()) == PLUGGED_TYPE_BUTT)) {
        g_batteryConnectOnce = false;
        return isSuccess;
    }

    if (g_batteryConnectOnce) {
        return isSuccess;
    }

    data.SetCode(BatteryInfo::COMMON_EVENT_CODE_PLUGGED_TYPE);
    data.SetData(ToString(static_cast<uint32_t>(info.GetPluggedType())));
    BATTERY_HILOGD(FEATURE_BATT_INFO, "publisher pluggedtype=%{public}d",
        static_cast<uint32_t>(info.GetPluggedType()));
    isSuccess = CommonEventManager::PublishCommonEvent(data, publishInfo);
    if (!isSuccess) {
        BATTERY_HILOGD(FEATURE_BATT_INFO, "failed to publish power_connected event");
    }

    g_batteryConnectOnce = true;
    return isSuccess;
}

bool BatteryServiceSubscriber::HandleBatteryPowerDisconnectedEvent(const BatteryInfo& info)
{
    Want want;
    want.SetAction(CommonEventSupport::COMMON_EVENT_POWER_DISCONNECTED);
    CommonEventData data;
    data.SetWant(want);
    CommonEventPublishInfo publishInfo;
    publishInfo.SetOrdered(false);
    bool isSuccess = true;

    if ((static_cast<uint32_t>(info.GetPluggedType()) != PLUGGED_TYPE_NONE) &&
        (static_cast<uint32_t>(info.GetPluggedType()) != PLUGGED_TYPE_BUTT)) {
        g_batteryDisconnectOnce = false;
        return isSuccess;
    }

    if (g_batteryDisconnectOnce) {
        return isSuccess;
    }

    data.SetCode(BatteryInfo::COMMON_EVENT_CODE_PLUGGED_TYPE);
    data.SetData(ToString(static_cast<uint32_t>(info.GetPluggedType())));
    BATTERY_HILOGD(FEATURE_BATT_INFO, "publisher pluggedtype=%{public}d",
        static_cast<uint32_t>(info.GetPluggedType()));
    isSuccess = CommonEventManager::PublishCommonEvent(data, publishInfo);
    if (!isSuccess) {
        BATTERY_HILOGD(FEATURE_BATT_INFO, "failed to publish power_disconnected event");
    }

    g_batteryDisconnectOnce = true;
    return isSuccess;
}

bool BatteryServiceSubscriber::HandleBatteryChargingEvent(const BatteryInfo& info)
{
    Want want;
    want.SetAction(CommonEventSupport::COMMON_EVENT_CHARGING);
    CommonEventData data;
    data.SetWant(want);
    CommonEventPublishInfo publishInfo;
    publishInfo.SetOrdered(false);
    bool isSuccess = true;

    if (static_cast<uint32_t>(info.GetChargeState()) !=
        static_cast<uint32_t>(BatteryChargeState::CHARGE_STATE_ENABLE)) {
        g_batteryChargingOnce = false;
        return isSuccess;
    }

    if (g_batteryChargingOnce) {
        return isSuccess;
    }

    data.SetCode(BatteryInfo::COMMON_EVENT_CODE_CHARGE_STATE);
    data.SetData(ToString(static_cast<uint32_t>(info.GetChargeState())));
    BATTERY_HILOGD(FEATURE_BATT_INFO, "publisher chargeState=%{public}d",
        static_cast<uint32_t>(info.GetChargeState()));
    isSuccess = CommonEventManager::PublishCommonEvent(data, publishInfo);
    if (!isSuccess) {
        BATTERY_HILOGD(FEATURE_BATT_INFO, "failed to publish battery charing event");
    }

    g_batteryChargingOnce = true;
    return isSuccess;
}

bool BatteryServiceSubscriber::HandleBatteryDischargingEvent(const BatteryInfo& info)
{
    Want want;
    want.SetAction(CommonEventSupport::COMMON_EVENT_DISCHARGING);
    CommonEventData data;
    data.SetWant(want);
    CommonEventPublishInfo publishInfo;
    publishInfo.SetOrdered(false);
    bool isSuccess = true;

    if (static_cast<uint32_t>(info.GetChargeState()) != static_cast<uint32_t>(BatteryChargeState::CHARGE_STATE_NONE)) {
        g_batteryDischargingOnce = false;
        return isSuccess;
    }

    if (g_batteryDischargingOnce) {
        return isSuccess;
    }

    data.SetCode(BatteryInfo::COMMON_EVENT_CODE_CHARGE_STATE);
    data.SetData(ToString(static_cast<uint32_t>(info.GetChargeState())));
    BATTERY_HILOGD(FEATURE_BATT_INFO, "publisher chargeState=%{public}d",
        static_cast<uint32_t>(info.GetChargeState()));
    isSuccess = CommonEventManager::PublishCommonEvent(data, publishInfo);
    if (!isSuccess) {
        BATTERY_HILOGD(FEATURE_BATT_INFO, "failed to publish battery charing event");
    }

    g_batteryDischargingOnce = true;
    return isSuccess;
}
} // namespace PowerMgr
} // namespace OHOS

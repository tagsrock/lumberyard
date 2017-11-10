/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#ifndef CRYINCLUDE_CRYAISYSTEM_AISIGNALCRCS_H
#define CRYINCLUDE_CRYAISYSTEM_AISIGNALCRCS_H
#pragma once

struct AISIGNALS_CRC
{
    //the ctor initializations are now moved into Init since the central crc generator access must be at a known time
    AISIGNALS_CRC();
    //initializes all crc dependent members, called in CAISystem ctor
    void Init();

    uint32 m_nOnRequestNewObstacle;
    uint32 m_nOnRequestUpdate;
    uint32 m_nOnRequestShootSpot;
    uint32 m_nOnRequestHideSpot;
    uint32 m_nOnStartTimer;
    uint32 m_nOnLastKnownPositionApproached;
    uint32 m_nOnEndApproach;
    uint32 m_nOnAbortAction;
    uint32 m_nOnActionCompleted;
    uint32 m_nOnNoPathFound;
    uint32 m_nOnApproachEnd;
    uint32 m_nOnEndFollow;
    uint32 m_nOnBulletRain;
    uint32 m_nOnBulletHit;
    uint32 m_nOnDriverEntered;
    uint32 m_nAIORD_ATTACK;
    uint32 m_nAIORD_SEARCH;
    uint32 m_nAIORD_REPORTDONE;
    uint32 m_nAIORD_REPORTFAIL;
    uint32 m_nOnChangeStance;
    uint32 m_nOnScaleFormation;
    uint32 m_nOnUnitDied;
    uint32 m_nOnUnitDamaged;
    uint32 m_nOnUnitBusy;
    uint32 m_nOnUnitSuspended;
    uint32 m_nOnUnitResumed;
    uint32 m_nOnSetUnitProperties;
    uint32 m_nOnJoinTeam;
    uint32 m_nRPT_LeaderDead;
    uint32 m_nOnSpotSeeingTarget;
    uint32 m_nOnSpotLosingTarget;
    uint32 m_nOnFormationPointReached;
    uint32 m_nOnKeepEnabled;
    uint32 m_nOnActionDone;
    uint32 m_nOnGroupAdvance;
    uint32 m_nOnGroupSeek;
    uint32 m_nOnLeaderReadabilitySeek;
    uint32 m_nOnGroupCover;
    uint32 m_nAddDangerPoint;
    uint32 m_nOnLeaderReadabilityAlarm;
    uint32 m_nOnAdvanceTargetCompromised;
    uint32 m_nOnGroupCohesionTest;
    uint32 m_nOnGroupTestReadabilityCohesion;
    uint32 m_nOnGroupTestReadabilityAdvance;
    uint32 m_nOnGroupAdvanceTest;
    uint32 m_nOnLeaderReadabilityAdvanceLeft;
    uint32 m_nOnLeaderReadabilityAdvanceRight;
    uint32 m_nOnLeaderReadabilityAdvanceForward;
    uint32 m_nOnGroupTurnAmbient;
    uint32 m_nOnGroupTurnAttack;
    uint32 m_nOnShapeEnabled;
    uint32 m_nOnShapeDisabled;
    uint32 m_nOnCloseContact;
    uint32 m_nOnTargetDead;
    uint32 m_nOnEndPathOffset;
    uint32 m_nOnPathFindAtStart;
    uint32 m_nOnPathFound;
    uint32 m_nOnBackOffFailed;
    uint32 m_nOnBadHideSpot;
    uint32 m_nOnCoverReached;
    uint32 m_nOnTPSDestinationNotFound;
    uint32 m_nOnTPSDestinationFound;
    uint32 m_nOnTPSDestinationReached;
    uint32 m_nOnRightLean;
    uint32 m_nOnLeftLean;
    uint32 m_nOnLowHideSpot;
    uint32 m_nOnChargeStart;
    uint32 m_nOnChargeHit;
    uint32 m_nOnChargeMiss;
    uint32 m_nOnChargeBailOut;
    uint32 m_nOnMovingToCover;
    uint32 m_nOnMovingInCover;
    uint32 m_nOnEnterCover;
    uint32 m_nOnLeaveCover;
    uint32 m_nOnCoverCompromised;
    uint32 m_nOnCoverLocationCompromised;
    uint32 m_nOnHighCover;
    uint32 m_nOnLowCover;
    uint32 m_nOnNoAimPosture;
    uint32 m_nOnNoPeekPosture;
    uint32 m_nORDER_IDLE;
    uint32 m_nOnNoGroupTarget;
    uint32 m_nOnLeaderTooFar;
    uint32 m_nOnEnemySteady;
    uint32 m_nOnShootSpotFound;
    uint32 m_nOnShootSpotNotFound;
    uint32 m_nOnLeaderStop;
    uint32 m_nOnUnitMoving;
    uint32 m_nOnUnitStop;
    uint32 m_nORDER_EXIT_VEHICLE;
    uint32 m_nOnNoFormationPoint;
    uint32 m_nOnTargetApproaching;
    uint32 m_nOnTargetFleeing;
    uint32 m_nOnNoTargetVisible;
    uint32 m_nOnNoTargetAwareness;
    uint32 m_nOnNoHidingPlace;
    uint32 m_nOnLowAmmo;
    uint32 m_nOnOutOfAmmo;
    uint32 m_nOnReloaded;
    uint32 m_nOnMeleeExecuted;
    uint32 m_nOnPlayerLooking;
    uint32 m_nOnPlayerSticking;
    uint32 m_nOnPlayerLookingAway;
    uint32 m_nOnPlayerGoingAway;
    uint32 m_nOnTargetTooClose;
    uint32 m_nOnTargetTooFar;
    uint32 m_nOnFriendInWay;
    uint32 m_nOnUseSmartObject;
    uint32 m_nOnAvoidDanger;
    uint32 m_nOnRequestUpdateTowards;
    uint32 m_nOnRequestUpdateAlternative;
    uint32 m_nOnClearSpotList;
    uint32 m_nOnSetPredictionTime;
    uint32 m_nOnInterestSystemEvent;
    uint32 m_nOnCheckDeadTarget;
    uint32 m_nOnCheckDeadBody;
    uint32 m_nOnSpecialAction;
    uint32 m_nOnNewAttentionTarget;
    uint32 m_nOnAttentionTargetThreatChanged;
};

#endif // CRYINCLUDE_CRYAISYSTEM_AISIGNALCRCS_H

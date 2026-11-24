/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file femail.c
 *     Front-end desktop and menu system, emails handling.
 * @par Purpose:
 *     Implement functions for emails handling in front-end desktop.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     22 Apr 2023 - 22 Oct 2023
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "femail.h"

#include "bfmemut.h"

#include "campaign.h"
#include "febrief.h"
#include "femain.h"
#include "misstat.h"
#include "swlog.h"
/******************************************************************************/

struct EmailItem email_store[EMAIL_STORE_COUNT];
struct NewMailItem newmail_store[NEWMAIL_STORE_COUNT];

ubyte new_mail = 0;
ushort next_email = 0;
short next_ref = 0;

/******************************************************************************/

ushort activate_queued_mail(void)
{
    struct EmailItem *p_mail;
    ushort bri, eml, nml;
    short nref;
    ushort mtype;

    if ((newmail_store[0].Flag & 0x0001) != 0)
    {
        bri = next_brief;
        nref = next_ref;
        next_ref = nref + 1;
        next_brief = bri + 1;
        p_mail = &brief_store[bri];

        p_mail->RefNum = nref;
        p_mail->RecvDay = global_date.Day;
        p_mail->RecvMonth = global_date.Month;
        p_mail->RecvYear = global_date.Year;
        p_mail->Mission = newmail_store[0].Mission;
        LbMemorySet(&mission_status[bri + 1], 0, sizeof(struct MissionStatus));

        bri = next_brief;
        mission_status[bri].Days = 0;
        mission_status[bri].Hours = 0;
        mtype = 1;
    }
    else
    {
        eml = next_email;
        nref = next_ref;
        next_email = eml + 1;
        next_ref = nref + 1;
        p_mail = &email_store[eml];

        p_mail->RefNum = next_ref;
        p_mail->RecvDay = newmail_store[0].RecvDay;
        p_mail->RecvMonth = newmail_store[0].RecvMonth;
        p_mail->RecvYear = newmail_store[0].RecvYear;
        p_mail->Mission = newmail_store[0].Mission;
        mtype = 0;
    }

    nml = new_mail;
    for (eml = 1; eml < nml; eml++)
    {
        newmail_store[eml - 1] = newmail_store[eml];
    }
    new_mail = nml - 1;

    return mtype;
}

void delete_mail(ushort mailnum, ubyte emtype)
{
    ushort i;

    if (emtype == MlTp_Mission)
    {
        for (i = mailnum + 1; i < next_brief; i++)
        {
            LbMemoryCopy(&brief_store[i - 1], &brief_store[i], sizeof(struct EmailItem));
            LbMemoryCopy(&mission_status[i - 1], &mission_status[i], sizeof(struct MissionStatus));
        }
        next_brief--;
    }
    else
    {
        for (i = mailnum + 1; i < next_email; i++)
        {
            LbMemoryCopy(&email_store[i - 1], &email_store[i], sizeof(struct EmailItem));
        }
        next_email--;
    }
}

void queue_up_new_mail(ubyte emtype, short missi)
{
    int i;

    LOGSYNC("New email type %d after mission %d, source %d", (int)emtype,
      (int)missi, (int)mission_list[missi].SourceID);
    if ((emtype == MlTp_Mission) && (mission_list[missi].SourceID == 0))
        return;
    if (missi < 0) {
        missi = -missi;
        emtype = MlTp_Email;
    }
    i = new_mail;
    newmail_store[i].Mission = missi;
    newmail_store[i].RecvDay = global_date.Day;
    newmail_store[i].RecvMonth = global_date.Month;
    newmail_store[i].RecvYear = global_date.Year;
    if (emtype != MlTp_Mission)
        newmail_store[i].Flag = 0x02;
    else
        newmail_store[i].Flag = 0x01;
    new_mail++;
}

/******************************************************************************/

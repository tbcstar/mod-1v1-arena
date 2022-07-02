/*
 *   Copyright (C) 2016+     AzerothCore <www.azerothcore.org>, released under GNU AGPL3 v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 *   Copyright (C) 2013      Emu-Devstore <http://emu-devstore.com/>
 *
 *   Written by Teiby <http://www.teiby.de/>
 *   Adjusted by fr4z3n for azerothcore
 *   Reworked by XDev
 */

#include "ScriptMgr.h"
#include "ArenaTeamMgr.h"
#include "DisableMgr.h"
#include "BattlegroundMgr.h"
#include "Battleground.h"
#include "BattlegroundQueue.h"
#include "ArenaTeam.h"
#include "Language.h"
#include "Config.h"
#include "Log.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "SharedDefines.h"
#include "Chat.h"

//Const for 1v1 arena
constexpr uint32 ARENA_TEAM_1V1 = 1;
constexpr uint32 ARENA_TYPE_1V1 = 1;
constexpr uint32 BATTLEGROUND_QUEUE_1V1 = 11;
constexpr BattlegroundQueueTypeId bgQueueTypeId = (BattlegroundQueueTypeId)((int)BATTLEGROUND_QUEUE_5v5 + 1);
uint32 ARENA_SLOT_1V1 = 3;

//Config
std::vector<uint32> forbiddenTalents;

class configloader_1v1arena : public WorldScript
{
public:
    configloader_1v1arena() : WorldScript("configloader_1v1arena") {}


    virtual void OnAfterConfigLoad(bool /*Reload*/) override
    {
        std::stringstream ss(sConfigMgr->GetOption<std::string>("Arena1v1.ForbiddenTalentsIDs", "0"));

        for (std::string blockedTalentsStr; std::getline(ss, blockedTalentsStr, ',');)
        {
            forbiddenTalents.push_back(stoi(blockedTalentsStr));
        }		
				
        ARENA_SLOT_1V1 = sConfigMgr->GetOption<uint32>("Arena1v1.ArenaSlotID", 3);
        
        ArenaTeam::ArenaSlotByType.emplace(ARENA_TEAM_1V1, ARENA_SLOT_1V1);
        ArenaTeam::ArenaReqPlayersForType.emplace(ARENA_TYPE_1V1, 2);
        
        BattlegroundMgr::queueToBg.insert({ BATTLEGROUND_QUEUE_1V1,   BATTLEGROUND_AA });
        BattlegroundMgr::QueueToArenaType.emplace(BATTLEGROUND_QUEUE_1V1, (ArenaType) ARENA_TYPE_1V1);
        BattlegroundMgr::ArenaTypeToQueue.emplace(ARENA_TYPE_1V1, (BattlegroundQueueTypeId) BATTLEGROUND_QUEUE_1V1);
    }

};

class playerscript_1v1arena : public PlayerScript
{
public:
    playerscript_1v1arena() : PlayerScript("playerscript_1v1arena") { }

    void OnLogin(Player* pPlayer) override
    {
        if (sConfigMgr->GetOption<bool>("Arena1v1.Announcer", true))
            ChatHandler(pPlayer->GetSession()).SendSysMessage("此服务器正在运行 |cff4CFF00竞技场 1v1 |r模块。");
    }


    void GetCustomGetArenaTeamId(const Player* player, uint8 slot, uint32& id) const override
    {
        if (slot == sConfigMgr->GetOption<uint32>("Arena1v1.ArenaSlotID", 3))
        {
            if (ArenaTeam* at = sArenaTeamMgr->GetArenaTeamByCaptain(player->GetGUID(), ARENA_TEAM_1V1))
            {
                id = at->GetId();
            }
        }
    }


    void GetCustomArenaPersonalRating(const Player* player, uint8 slot, uint32& rating) const override
    {
        if (slot == sConfigMgr->GetOption<uint32>("Arena1v1.ArenaSlotID", 3))
        {
            if (ArenaTeam* at = sArenaTeamMgr->GetArenaTeamByCaptain(player->GetGUID(), ARENA_TEAM_1V1))
            {
                rating = at->GetRating();
            }
        }
    }


    void OnGetMaxPersonalArenaRatingRequirement(const Player* player, uint32 minslot, uint32& maxArenaRating) const override
    {
        if (sConfigMgr->GetOption<bool>("Arena1v1.VendorRating", false) && minslot < (uint32)sConfigMgr->GetOption<uint32>("Arena1v1.ArenaSlotID", 3))
        {
            if (ArenaTeam* at = sArenaTeamMgr->GetArenaTeamByCaptain(player->GetGUID(), ARENA_TEAM_1V1))
            {
                maxArenaRating = std::max(at->GetRating(), maxArenaRating);
            }
        }
    }
};

class npc_1v1arena : public CreatureScript
{
public:
    npc_1v1arena() : CreatureScript("npc_1v1arena") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!player || !creature)
            return true;

        if (sConfigMgr->GetOption<bool>("Arena1v1.Enable", true) == false)
        {
            ChatHandler(player->GetSession()).SendSysMessage("1v1 disabled!");
            return true;
        }

        if (player->InBattlegroundQueueForBattlegroundQueueType(bgQueueTypeId))
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "取消1v1竞技场排队", GOSSIP_SENDER_MAIN, 3, "你确定吗？", 0, false);
        }
        else
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "排队进入1v1竞技场（未评级）", GOSSIP_SENDER_MAIN, 20);
        }

        if (!player->GetArenaTeamId(ArenaTeam::GetSlotByType(ARENA_TEAM_1V1)))
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "创建新的 1v1 竞技场团队", GOSSIP_SENDER_MAIN, 1, "你确定吗？", sConfigMgr->GetOption<uint32>("Arena1v1.Costs", 400000), false);
        }
        else
        {
            if (!player->InBattlegroundQueueForBattlegroundQueueType(bgQueueTypeId))
            {
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, "排队进入1v1竞技场（额定）", GOSSIP_SENDER_MAIN, 2);
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, "解散竞技场团队", GOSSIP_SENDER_MAIN, 5, "你确定吗？", 0, false);
            }

            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "显示您的统计数据", GOSSIP_SENDER_MAIN, 4);
        }

        SendGossipMenuFor(player, 68, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        if (!player || !creature)
            return true;

        ClearGossipMenuFor(player);

        ChatHandler handler(player->GetSession());

        switch (action)
        {
        case 1: // Create new Arenateam
        {
            if (sConfigMgr->GetOption<uint32>("Arena1v1.MinLevel", 80) <= player->getLevel())
            {
                if (player->GetMoney() >= uint32(sConfigMgr->GetOption<uint32>("Arena1v1.Costs", 400000)) && CreateArenateam(player, creature))
                    player->ModifyMoney(sConfigMgr->GetOption<uint32>("Arena1v1.Costs", 400000) * -1);
            }
            else
            {
                handler.PSendSysMessage("您必须达到%u +级别才能创建 1v1 竞技场团队。", sConfigMgr->GetOption<uint32>("Arena1v1.MinLevel", 70));
                CloseGossipMenuFor(player);
                return true;
            }
        }
        break;

        case 2: // Join Queue Arena (rated)
        {
            if (Arena1v1CheckTalents(player) && !JoinQueueArena(player, creature, true))
                handler.SendSysMessage("加入队列时出现问题。");

            CloseGossipMenuFor(player);
            return true;
        }
        break;

        case 20: // Join Queue Arena (unrated)
        {
            if (Arena1v1CheckTalents(player) && !JoinQueueArena(player, creature, false))
                handler.SendSysMessage("加入队列时出现问题。");

            CloseGossipMenuFor(player);
            return true;
        }
        break;

        case 3: // Leave Queue
        {
            uint8 arenaType = ARENA_TYPE_1V1;

            if (!player->InBattlegroundQueueForBattlegroundQueueType(bgQueueTypeId))
                return true;

            WorldPacket data;
            data << arenaType << (uint8)0x0 << (uint32)BATTLEGROUND_AA << (uint16)0x0 << (uint8)0x0;
            player->GetSession()->HandleBattleFieldPortOpcode(data);
            CloseGossipMenuFor(player);
            return true;
        }
        break;

        case 4: // get statistics
        {
            ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(player->GetArenaTeamId(ArenaTeam::GetSlotByType(ARENA_TEAM_1V1)));
            if (at)
            {
                std::stringstream s;
                s << "评分: " << at->GetStats().Rating;
                s << "\nRank: " << at->GetStats().Rank;
                s << "\nSeason Games: " << at->GetStats().SeasonGames;
                s << "\n赛季胜利: " << at->GetStats().SeasonWins;
                s << "\nWeek Games: " << at->GetStats().WeekGames;
                s << "\n本周胜利: " << at->GetStats().WeekWins;

                ChatHandler(player->GetSession()).PSendSysMessage(SERVER_MSG_STRING, s.str().c_str());
            }
        }
        break;

        case 5: // Disband arenateam
        {
            WorldPacket Data;
            Data << player->GetArenaTeamId(ArenaTeam::GetSlotByType(ARENA_TEAM_1V1));
            player->GetSession()->HandleArenaTeamLeaveOpcode(Data);
            handler.SendSysMessage("Arenateam deleted!");
            CloseGossipMenuFor(player);
            return true;
        }
        break;
        }

        OnGossipHello(player, creature);
        return true;
    }

private:
    bool JoinQueueArena(Player* player, Creature* me, bool isRated)
    {
        if (!player || !me)
            return false;

        if (sConfigMgr->GetOption<uint32>("Arena1v1.MinLevel", 80) > player->getLevel())
            return false;

        uint8 arenaslot = ArenaTeam::GetSlotByType(ARENA_TEAM_1V1);
        uint8 arenatype = ARENA_TYPE_1V1;
        uint32 arenaRating = 0;
        uint32 matchmakerRating = 0;

        // ignore if we already in BG or BG queue
        if (player->InBattleground())
            return false;

        //check existance
        Battleground* bg = sBattlegroundMgr->GetBattlegroundTemplate(BATTLEGROUND_AA);
        if (!bg)
        {
            LOG_ERROR("module", "Battleground: template bg (all arenas) not found");
            return false;
        }

        if (DisableMgr::IsDisabledFor(DISABLE_TYPE_BATTLEGROUND, BATTLEGROUND_AA, nullptr))
        {
            ChatHandler(player->GetSession()).PSendSysMessage(LANG_ARENA_DISABLED);
            return false;
        }

        PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(bg->GetMapId(), player->getLevel());
        if (!bracketEntry)
            return false;

        // check if already in queue
        if (player->GetBattlegroundQueueIndex(bgQueueTypeId) < PLAYER_MAX_BATTLEGROUND_QUEUES)
            return false; // //player is already in this queue

        // check if has free queue slots
        if (!player->HasFreeBattlegroundQueueId())
            return false;

        uint32 ateamId = 0;

        if (isRated)
        {
            ateamId = player->GetArenaTeamId(arenaslot);
            ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(ateamId);
            if (!at)
            {
                player->GetSession()->SendNotInArenaTeamPacket(arenatype);
                return false;
            }

            // get the team rating for queueing
            arenaRating = at->GetRating();
            matchmakerRating = arenaRating;
            // the arenateam id must match for everyone in the group

            if (arenaRating <= 0)
                arenaRating = 1;
        }

        BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);
        BattlegroundTypeId bgTypeId = BATTLEGROUND_AA;

        bg->SetRated(isRated);
        bg->SetMaxPlayersPerTeam(1);

        GroupQueueInfo* ginfo = bgQueue.AddGroup(player, nullptr, bgTypeId, bracketEntry, arenatype, isRated != 0, false, arenaRating, matchmakerRating, ateamId, 0);
        uint32 avgTime = bgQueue.GetAverageQueueWaitTime(ginfo);
        uint32 queueSlot = player->AddBattlegroundQueueId(bgQueueTypeId);

        // send status packet (in queue)
        WorldPacket data;
        sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, bg, queueSlot, STATUS_WAIT_QUEUE, avgTime, 0, arenatype, TEAM_NEUTRAL, isRated);
        player->GetSession()->SendPacket(&data);

        sBattlegroundMgr->ScheduleQueueUpdate(matchmakerRating, arenatype, bgQueueTypeId, bgTypeId, bracketEntry->GetBracketId());

        return true;
    }

    bool CreateArenateam(Player* player, Creature* me)
    {
        if (!player || !me)
            return false;

        uint8 slot = ArenaTeam::GetSlotByType(ARENA_TEAM_1V1);
        //Just to make sure as some other module might edit this value
        if (slot == 0)
            return false;

        // Check if player is already in an arena team
        if (player->GetArenaTeamId(slot))
        {
            player->GetSession()->SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, player->GetName(), "你已经在一个竞技场团队中了！", ERR_ALREADY_IN_ARENA_TEAM);
            return false;
        }

        // Teamname = playername
        // if teamname exist, we have to choose another name (playername + number)
        int i = 1;
        std::stringstream teamName;
        teamName << player->GetName();
        do
        {
            if (sArenaTeamMgr->GetArenaTeamByName(teamName.str()) != NULL) // teamname exist, so choose another name
            {
                teamName.str(std::string());
                teamName << player->GetName() << (i++);
            }
            else
                break;
        } while (i < 100); // should never happen

        // Create arena team
        ArenaTeam* arenaTeam = new ArenaTeam();
        if (!arenaTeam->Create(player->GetGUID(), ARENA_TEAM_1V1, teamName.str(), 4283124816, 45, 4294242303, 5, 4294705149))
        {
            delete arenaTeam;
            return false;
        }

        // Register arena team
        sArenaTeamMgr->AddArenaTeam(arenaTeam);

        ChatHandler(player->GetSession()).SendSysMessage("1v1竞技场战队创建成功！");

        return true;
    }

    bool Arena1v1CheckTalents(Player* player)
    {
        if (!player)
            return false;

        if (sConfigMgr->GetOption<bool>("Arena1v1.BlockForbiddenTalents", true) == false)
            return true;

        uint32 count = 0;

        for (uint32 talentId = 0; talentId < sTalentStore.GetNumRows(); ++talentId)
        {
            TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);

            if (!talentInfo)
                continue;

            if (std::find(forbiddenTalents.begin(), forbiddenTalents.end(), talentInfo->TalentID) != forbiddenTalents.end())
            {
                ChatHandler(player->GetSession()).SendSysMessage("你不能加入，因为你有禁忌的天赋。");
                return false;
            }

            for (int8 rank = MAX_TALENT_RANK - 1; rank >= 0; --rank)
                if (talentInfo->RankID[rank] == 0)
                    continue;
        }

        if (count >= 36)
        {
            ChatHandler(player->GetSession()).SendSysMessage("你不能加入，因为你的禁忌天赋点太多。 （治疗/坦克）");
            return false;
        }

        return true;
    }
};

class team_1v1arena : public ArenaTeamScript
{
public:
    team_1v1arena() : ArenaTeamScript("team_1v1arena") {}


    void OnGetSlotByType(const uint32 type, uint8& slot) override
    {
        if (type == ARENA_TEAM_1V1)
        {
            slot = sConfigMgr->GetOption<uint32>("Arena1v1.ArenaSlotID", 3);
        }
    }


    void OnGetArenaPoints(ArenaTeam* at, float& points) override
    {
        if (at->GetType() == ARENA_TEAM_1V1)
        {
            points *= sConfigMgr->GetOption<float>("Arena1v1.ArenaPointsMulti", 0.64f);
        }
    }


    void OnTypeIDToQueueID(const BattlegroundTypeId, const uint8 arenaType, uint32& _bgQueueTypeId) override
    {
        if (arenaType == ARENA_TYPE_1V1)
        {
            _bgQueueTypeId = bgQueueTypeId;
        }
    }


    void OnQueueIdToArenaType(const BattlegroundQueueTypeId _bgQueueTypeId, uint8& arenaType) override
    {
        if (_bgQueueTypeId == bgQueueTypeId)
        {
            arenaType = ARENA_TYPE_1V1;
        }
    }


    void OnSetArenaMaxPlayersPerTeam(const uint8 type, uint32& maxPlayersPerTeam) override
    {
        if (type == ARENA_TYPE_1V1)
        {
            maxPlayersPerTeam = 1;
        }
    }

};

void AddSC_npc_1v1arena()
{
    new configloader_1v1arena();
    new playerscript_1v1arena();
    new npc_1v1arena();
    new team_1v1arena();
}

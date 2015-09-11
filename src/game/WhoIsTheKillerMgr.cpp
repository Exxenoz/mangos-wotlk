#include "Common.h"
#include "Config/Config.h"
#include "SystemConfig.h"
#include "WhoIsTheKillerSpiel.h"
#include "WhoIsTheKillerMgr.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "GossipDef.h"
#include "Log.h"
#include "World.h"

#include "Policies/SingletonImp.h"

INSTANTIATE_SINGLETON_1( WhoIsTheKillerMgr );

WhoIsTheKillerMgr::WhoIsTheKillerMgr()
{
    m_uiMinutenTimer = 60000; // 60 Sekunden
    m_uiSekundenTimer = 1000; // 1 Sekunde
    m_uiSpieleIndexZaehler = 0;
    m_mSpiele.clear();
}

WhoIsTheKillerMgr::~WhoIsTheKillerMgr()
{
    for (SpieleMap::const_iterator itr = m_mSpiele.begin(); itr != m_mSpiele.end(); ++itr)
        delete itr->second;

    m_mSpiele.clear();
}

void WhoIsTheKillerMgr::EinstellungenLaden()
{
    sLog.outString(">> Lade Who is the Killer Einstellungen...");

    m_iEinstellungen[COUNTDOWN] = sConfig.GetIntDefault("WhoIsTheKiller.Countdown", 10);
    if (m_iEinstellungen[COUNTDOWN] <= 0)
    {
        sLog.outError("Countdown darf nicht kleiner gleich 0 sein. Default: 10");
        m_iEinstellungen[COUNTDOWN] = 10;
    }

    m_iEinstellungen[PHASE] = sConfig.GetIntDefault("WhoIsTheKiller.Phase", 1024);
    if (m_iEinstellungen[PHASE] <= 0)
    {
        sLog.outError("Phase darf nicht kleiner gleich 0 sein. Default: 1024");
        m_iEinstellungen[PHASE] = 1024;
    }

    m_iEinstellungen[CAMPER_MIN] = sConfig.GetIntDefault("WhoIsTheKiller.CamperMin", 1);
    if (m_iEinstellungen[CAMPER_MIN] <= 0)
    {
        sLog.outError("CamperMin darf nicht kleiner gleich 0 sein. Default: 1");
        m_iEinstellungen[CAMPER_MIN] = 1;
    }

    m_iEinstellungen[CAMPER_MODEL] = sConfig.GetIntDefault("WhoIsTheKiller.CamperModel", 17330);
    if (m_iEinstellungen[CAMPER_MODEL] <= 0)
    {
        sLog.outError("CamperModel darf nicht kleiner gleich 0 sein. Default: 17330");
        m_iEinstellungen[CAMPER_MODEL] = 17330;
    }

    m_iEinstellungen[CAMPER_WBLT] = sConfig.GetIntDefault("WhoIsTheKiller.CamperWiederbelebungsTimer", 5);
    if (m_iEinstellungen[CAMPER_WBLT] <= 0)
    {
        sLog.outError("CamperWiederbelebungsTimer darf nicht kleiner gleich 0 sein. Default: 5");
        m_iEinstellungen[CAMPER_WBLT] = 5;
    }

    m_iEinstellungen[KILLER_MODEL] = sConfig.GetIntDefault("WhoIsTheKiller.KillerModel", 17311);
    if (m_iEinstellungen[KILLER_MODEL] <= 0)
    {
        sLog.outError("KillerModel darf nicht kleiner gleich 0 sein. Default: 17311");
        m_iEinstellungen[KILLER_MODEL] = 17311;
    }

    m_iEinstellungen[KILLER_GESCHW_BODEN] = sConfig.GetIntDefault("WhoIsTheKiller.KillerGeschwBoden", 3);
    if (m_iEinstellungen[KILLER_GESCHW_BODEN] <= 0)
    {
        sLog.outError("KillerGeschwBoden darf nicht kleiner gleich 0 sein. Default: 3");
        m_iEinstellungen[KILLER_GESCHW_BODEN] = 3;
    }

    m_iEinstellungen[KILLER_GESCHW_FLUG] = sConfig.GetIntDefault("WhoIsTheKiller.KillerGeschwFlug", 5);
    if (m_iEinstellungen[KILLER_GESCHW_FLUG] <= 0)
    {
        sLog.outError("KillerGeschwFlug darf nicht kleiner gleich 0 sein. Default: 5");
        m_iEinstellungen[KILLER_GESCHW_FLUG] = 5;
    }

    m_iEinstellungen[KILLER_CD_KILLEN] = sConfig.GetIntDefault("WhoIsTheKiller.KillerfCooldownKillen", 3);
    if (m_iEinstellungen[KILLER_CD_KILLEN] < 0)
    {
        sLog.outError("KillerfCooldownKillen darf nicht kleiner 0 sein. Default: 3");
        m_iEinstellungen[KILLER_CD_KILLEN] = 3;
    }

    m_iEinstellungen[KILLER_SIG_ICON] = sConfig.GetIntDefault("WhoIsTheKiller.KillerSignalIcon", 41);
    if (m_iEinstellungen[KILLER_SIG_ICON] <= 0)
    {
        sLog.outError("KillerSignalIcon darf nicht kleiner gleich 0 sein. Default: 41");
        m_iEinstellungen[KILLER_SIG_ICON] = 41;
    }

    m_iEinstellungen[KILLER_SIG_IV] = sConfig.GetIntDefault("WhoIsTheKiller.KillerSignalInterval", 10);
    if (m_iEinstellungen[KILLER_SIG_IV] < 0)
    {
        sLog.outError("KillerSignalInterval darf nicht kleiner 0 sein. Default: 10");
        m_iEinstellungen[KILLER_SIG_IV] = 10;
    }

    m_iEinstellungen[GM_GEWONNEN] = sConfig.GetIntDefault("WhoIsTheKiller.GMGewonnen", 1);
    if (m_iEinstellungen[GM_GEWONNEN] < 0)
    {
        sLog.outError("GMGewonnen darf nicht kleiner 0 sein. Default: 1");
        m_iEinstellungen[GM_GEWONNEN] = 1;
    }

    m_iEinstellungen[EV_GEWONNEN] = sConfig.GetIntDefault("WhoIsTheKiller.EVGewonnen", 3);
    if (m_iEinstellungen[GM_GEWONNEN] < 0)
    {
        sLog.outError("EVGewonnen darf nicht kleiner 0 sein. Default: 3");
        m_iEinstellungen[EV_GEWONNEN] = 3;
    }

    m_iEinstellungen[EV_UNENTSCHIEDEN] = sConfig.GetIntDefault("WhoIsTheKiller.EVUnentschieden", 1);
    if (m_iEinstellungen[EV_UNENTSCHIEDEN] < 0)
    {
        sLog.outError("EVUnentschieden darf nicht kleiner 0 sein. Default: 1");
        m_iEinstellungen[EV_UNENTSCHIEDEN] = 1;
    }

    m_iEinstellungen[EV_VERLOREN] = sConfig.GetIntDefault("WhoIsTheKiller.EVVerloren", 0);
    if (m_iEinstellungen[EV_VERLOREN] < 0)
    {
        sLog.outError("EVVerloren darf nicht kleiner 0 sein. Default: 0");
        m_iEinstellungen[EV_VERLOREN] = 0;
    }

    m_iEinstellungen[ZONEN_T_WECHSEL] = sConfig.GetIntDefault("WhoIsTheKiller.fZonenTimerWechsel", 5);
    if (m_iEinstellungen[ZONEN_T_WECHSEL] <= 0)
    {
        sLog.outError("ZonenTimerWechsel darf nicht kleiner gleich 0 sein. Default: 5");
        m_iEinstellungen[ZONEN_T_WECHSEL] = 5;
    }

    m_iEinstellungen[AKTIONS_ENTFERNUNG_MAX] = sConfig.GetIntDefault("WhoIsTheKiller.AktionsEntfernungMax", 25);
    if (m_iEinstellungen[AKTIONS_ENTFERNUNG_MAX] <= 0)
    {
        sLog.outError("AktionsEntfernungMax darf nicht kleiner gleich 0 sein. Default: 25");
        m_iEinstellungen[AKTIONS_ENTFERNUNG_MAX] = 25;
    }

    m_iEinstellungen[MAX_ERSTELLTE_SPIELE] = sConfig.GetIntDefault("WhoIsTheKiller.MaxErstellteSpiele", 10);
    if (m_iEinstellungen[MAX_ERSTELLTE_SPIELE] < 0)
    {
        sLog.outError("MaxErstellteSpiele darf nicht kleiner 0 sein. Default: 10");
        m_iEinstellungen[MAX_ERSTELLTE_SPIELE] = 10;
    }

    m_iEinstellungen[MAX_SPIELER_ANZAHL] = sConfig.GetIntDefault("WhoIsTheKiller.MaxSpielerAnzahl", 10);
    if (m_iEinstellungen[MAX_SPIELER_ANZAHL] < 2)
    {
        sLog.outError("MaxSpielerAnzahl darf nicht kleiner 2 sein. Default: 10");
        m_iEinstellungen[MAX_SPIELER_ANZAHL] = 10;
    }

    m_iEinstellungen[MAX_ANZAHL_LEBEN] = sConfig.GetIntDefault("WhoIsTheKiller.MaxAnzahlLeben", 10);
    if (m_iEinstellungen[MAX_ANZAHL_LEBEN] < 1)
    {
        sLog.outError("MaxAnzahlLeben darf nicht kleiner 1 sein. Default: 10");
        m_iEinstellungen[MAX_ANZAHL_LEBEN] = 10;
    }

    m_iEinstellungen[MAX_ANZAHL_KILLER] = sConfig.GetIntDefault("WhoIsTheKiller.MaxAnzahlKiller", 3);
    if (m_iEinstellungen[MAX_ANZAHL_KILLER] < 1)
    {
        sLog.outError("MaxAnzahlKiller darf nicht kleiner 1 sein. Default: 3");
        m_iEinstellungen[MAX_ANZAHL_KILLER] = 3;
    }

    m_iEinstellungen[MAX_ZEICHEN_SPIEL_NAME] = sConfig.GetIntDefault("WhoIsTheKiller.MaxZeichenSpielname", 16);
    if (m_iEinstellungen[MAX_ZEICHEN_SPIEL_NAME] < 1)
    {
        sLog.outError("MaxZeichenSpielname darf nicht kleiner 1 sein. Default: 16");
        m_iEinstellungen[MAX_ZEICHEN_SPIEL_NAME] = 16;
    }
}

// Who is the Killer Update Funktion
void WhoIsTheKillerMgr::Update(uint32 uiDiff)
{
    if (m_uiMinutenTimer < uiDiff)
    {
        for (SpieleMap::iterator itr = m_mSpiele.begin(); itr != m_mSpiele.end();)
        {
            // Wenn das Spiel zu Ende ist
            if (itr->second->GetSpielStatus() == STATUS_ENDE
            // oder der Leiter in STATUS_WARTEN Offline ist
            || (itr->second->GetSpielStatus() == STATUS_WARTEN && !GetSpieler(itr->second->GetLeiterGuid())))
            {
                delete itr->second;
                m_mSpiele.erase(itr++);
            }
            else
                ++itr;
        }

        m_uiMinutenTimer = 60000;
    }
    else
        m_uiMinutenTimer -= uiDiff;

    if (m_uiSekundenTimer < uiDiff)
    {
        //sLog.outDebug("WHO_IS_THE_KILLER_DEBUG_MESSAGE");

        for (SpieleMap::const_iterator itr = m_mSpiele.begin(); itr != m_mSpiele.end(); ++itr)
        {
            itr->second->Update();
        }

        m_uiSekundenTimer = 1000;
    }
    else
        m_uiSekundenTimer -= uiDiff;
}

// Erstellt ein Spiel mit der Guid des Leiters "LeiterGuid", dem Spielnamen "cSpielName", und gibt die SpielId zurueck
WhoIsTheKillerSpiel* WhoIsTheKillerMgr::SpielErstellen(ObjectGuid LeiterGuid, const char* cSpielName)
{
    WhoIsTheKillerSpiel* pNeuesSpiel = new WhoIsTheKillerSpiel(LeiterGuid, cSpielName);

    m_mSpiele[m_uiSpieleIndexZaehler] = pNeuesSpiel;

    // Debug
    sLog.outDebug(">> Spiel '%u' erstellt!", m_uiSpieleIndexZaehler);

    ++m_uiSpieleIndexZaehler;

    return pNeuesSpiel;
}

// Entfernt das Spiel mit der SpielId "uiSpielId"
void WhoIsTheKillerMgr::SpielEntfernen(WhoIsTheKillerSpiel* pSpiel)
{
    for (SpieleMap::iterator itr = m_mSpiele.begin(); itr != m_mSpiele.end(); ++itr)
    {
        // Pointer vergleichen...
        if (itr->second == pSpiel)
        {
            delete itr->second;
            m_mSpiele.erase(itr);
            break;
        }
    }

    // Debug
    sLog.outDebug(">> Spiel entfernt.");
}

WhoIsTheKillerSpiel* WhoIsTheKillerMgr::GetSpiel(ObjectGuid Guid)
{
    for (SpieleMap::const_iterator itr = m_mSpiele.begin(); itr != m_mSpiele.end(); ++itr)
    {
        if (itr->second->IstAlsSpielerAngemeldet(Guid))
            return itr->second;
    }

    return NULL;
}

WhoIsTheKillerSpiel* WhoIsTheKillerMgr::GetSpiel(uint8 uiSpielId)
{
    SpieleMap::const_iterator itr = m_mSpiele.find(uiSpielId);

    // Spiel nicht gefunden
    if (itr == m_mSpiele.end())
        return NULL;

    return itr->second;
}

// Erstellt eine Liste der erstellten Spiele
void WhoIsTheKillerMgr::ErstelleSpieleListe(Player* pPlayer, uint8 uiIconId, uint32 uiSender)
{
    for (SpieleMap::const_iterator itr = m_mSpiele.begin(); itr != m_mSpiele.end(); ++itr)
    {
        // Bereits gestartete Spiele werden nicht angezeigt, ausser der Spieler ist ein GM
        if (itr->second->GetSpielStatus() == STATUS_WARTEN || pPlayer->GetSession()->GetSecurity() >= SEC_GAMEMASTER)
            pPlayer->PlayerTalkClass->GetGossipMenu().AddMenuItem(uiIconId, itr->second->GetSpielName().c_str(), uiSender, itr->first, "", 0);
    }
}

// Schickt den Spieler mit der Guid "Guid" zurueck
Player* WhoIsTheKillerMgr::GetSpieler(ObjectGuid Guid)
{
    return HashMapHolder<Player>::Find(Guid);;
}

MANGOS_DLL_SPEC WhoIsTheKillerSpiel* sWhoIsTheKillerMgrSD2::GetSpiel(ObjectGuid Guid)
{
    return sWhoIsTheKillerMgr.GetSpiel(Guid);
}

MANGOS_DLL_SPEC WhoIsTheKillerSpiel* sWhoIsTheKillerMgrSD2::GetSpiel(uint8 uiSpielId)
{
    return sWhoIsTheKillerMgr.GetSpiel(uiSpielId);
}

MANGOS_DLL_SPEC uint8 sWhoIsTheKillerMgrSD2::GetSpieleAnzahl()
{
    return sWhoIsTheKillerMgr.GetSpieleAnzahl();
}

MANGOS_DLL_SPEC WhoIsTheKillerSpiel* sWhoIsTheKillerMgrSD2::SpielErstellen(ObjectGuid LeiterGuid, const char* cSpielName)
{
    return sWhoIsTheKillerMgr.SpielErstellen(LeiterGuid, cSpielName);
}

MANGOS_DLL_SPEC void sWhoIsTheKillerMgrSD2::SpielEntfernen(WhoIsTheKillerSpiel* pSpiel)
{
    sWhoIsTheKillerMgr.SpielEntfernen(pSpiel);
}

MANGOS_DLL_SPEC void sWhoIsTheKillerMgrSD2::ErstelleSpieleListe(Player* pPlayer, uint8 uiIconId, uint32 uiSender)
{
    sWhoIsTheKillerMgr.ErstelleSpieleListe(pPlayer, uiIconId, uiSender);
}

MANGOS_DLL_SPEC int32 sWhoIsTheKillerMgrSD2::GetEinstellung(uint8 uiIdx)
{
    return sWhoIsTheKillerMgr.GetEinstellung(uiIdx);
}

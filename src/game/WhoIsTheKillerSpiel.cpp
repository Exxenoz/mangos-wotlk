#include "Common.h"
#include "WhoIsTheKillerMgr.h"
#include "WhoIsTheKillerSpiel.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "SpellAuras.h"
#include "Player.h"
#include "GossipDef.h"
#include "Log.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"

WhoIsTheKillerSpiel::WhoIsTheKillerSpiel(ObjectGuid LeiterGuid, std::string sSpielName)
{
    m_LeiterGuid = LeiterGuid;
    m_sSpielName = sSpielName;
    m_mSpielerGuids[LeiterGuid] = true;

    // Figuren
    m_mKiller.clear();
    m_mCamper.clear();

    // Timer
    m_uiCountdown = sWhoIsTheKillerMgr.GetEinstellung(COUNTDOWN);
    m_uiSpielZeit = 0;
    m_uiSignalTimer = sWhoIsTheKillerMgr.GetEinstellung(KILLER_SIG_IV);
    m_uiSchlussTimer = 30; // 30 Sekunden

    // Optionen
    m_uiStatusKillenEnde = 600; // 10 Minuten
    m_uiStatusFluchtEnde = 360; // 6 Minuten
    m_uiKillerAnzahl = 1;
    m_uiLebenAnzahl = 1;

    // Spielstatus
    m_Status = STATUS_WARTEN;
}

WhoIsTheKillerSpiel::~WhoIsTheKillerSpiel()
{
}

// Wird jede Sekunde (!) aufgerufen
void WhoIsTheKillerSpiel::Update()
{
    switch (GetSpielStatus())
    {
        case STATUS_WARTEN:
            break;
        case STATUS_COUNTDOWN:
            if (!m_uiCountdown)
            {
                // Das Spiel beginnt...
                NachrichtSenden(WITK_TEXTID_COUNTDOWN_ENDE, true, false, NULL);
                SetSpielStatus(STATUS_KILLEN);
            }
            else
            {
                char cNachricht[100];
                sprintf(cNachricht, GetNachricht(WITK_TEXTID_COUNTDOWN_X), m_uiCountdown);
                NachrichtSenden(cNachricht, true, false, NULL);
                m_uiCountdown--;
            }
            break;
        case STATUS_KILLEN:
            if (!m_uiSignalTimer)
            {
                for (KillerMap::const_iterator itr = m_mKiller.begin(); itr != m_mKiller.end(); ++itr)
                    m_mKiller[itr->first].vAktionThreads.push_back(9);

                m_uiSignalTimer = sWhoIsTheKillerMgr.GetEinstellung(KILLER_SIG_IV);
            }
            else
                m_uiSignalTimer--;

            SpielGeschehen();

            m_uiSpielZeit++;

            // STATUS_ABSTIMMEN wird nach einer bestimmten Zeit gestartet
            if (GetSpielZeit() >= m_uiStatusKillenEnde)
                SetSpielStatus(STATUS_ABSTIMMEN);

            break;
        case STATUS_ABSTIMMEN:
            SpielGeschehen();

            m_uiSpielZeit++;

            // STATUS_FLUCHT wird nach 60 Sekunden gestartet
            if (GetSpielZeit() >= (m_uiStatusKillenEnde + 60))
                SetSpielStatus(STATUS_FLUCHT);

            break;
        case STATUS_FLUCHT:
            SpielGeschehen();

            m_uiSpielZeit++;

            // STATUS_SCHLUSS wird nach einer bestimmten Zeit gestartet
            if (GetSpielZeit() >= (m_uiStatusKillenEnde + 60 + m_uiStatusFluchtEnde))
            {
                // Das Spiel endet unentschieden
                for (KillerMap::const_iterator itr = m_mKiller.begin(); itr != m_mKiller.end(); ++itr)
                {
                    if (Player* pKiller = GetSpieler(itr->first))
                    {
                        CamperModifikationenEntfernen(pKiller);
                        // Nachricht...
                        NachrichtSenden(WITK_TEXTID_UNENTSCHIEDEN, true, true, pKiller);
                        // Der Killer bekommt eine Belohnung
                        SpielerBelohnung(pKiller, STATUS_UNENTSCHIEDEN);
                    }
                }

                for (CamperMap::const_iterator itr = m_mCamper.begin(); itr != m_mCamper.end(); ++itr)
                {
                    if (Player* pCamper = GetSpieler(itr->first))
                    {
                        // Nachricht...
                        NachrichtSenden(WITK_TEXTID_UNENTSCHIEDEN, true, true, pCamper);
                        // Der Camper bekommt eine Belohnung
                        SpielerBelohnung(pCamper, STATUS_UNENTSCHIEDEN);
                    }
                }

                SetSpielStatus(STATUS_SCHLUSS);
            }

            break;
        case STATUS_SCHLUSS:
            if (SchlussCountdown())
                SpielBeenden();
            break;
        case STATUS_ENDE:
            break;
    }
}

// Diese Funktion startet das Spiel
bool WhoIsTheKillerSpiel::SpielStarten()
{
    // Der Spielleiter darf ab hier Offline sein,
    // weil das Spiel bereits gestartet wurde

    // Wenn zu wenige Spieler fuer die
    // Figurenverteilung verfuegbar sind
    // wird "false" zurueckgeschickt
    if (!SpielFigurenZuteilen())
    {
        m_mKiller.clear();
        m_mCamper.clear();
        return false;
    }

    // Der Countdown wird gestartet :)
    SetSpielStatus(STATUS_COUNTDOWN);

    return true;
}

// Den Spielern wird eine Figur zugeteilt
bool WhoIsTheKillerSpiel::SpielFigurenZuteilen()
{
    // Zuerst alle Spieler auf Onlinestatus pruefen
    for (SpielerMap::iterator itr = m_mSpielerGuids.begin(); itr != m_mSpielerGuids.end();)
    {
        // Der Spieler ist nicht da,
        // also verlaesst er das Spiel
        if (!GetSpieler(itr->first))
        {
            m_mSpielerGuids.erase(itr++);
        }
        else
            ++itr;
    }

    SpielerMap mSpielerGuidsTemp = m_mSpielerGuids;

    // Es sind nicht genug Spieler vorhanden, um die Figuren zu verteilen
    // Wir brauchen die minimale Anzahl von Campern und die Anzahl der Killer
    if (mSpielerGuidsTemp.size() < uint8(sWhoIsTheKillerMgr.GetEinstellung(CAMPER_MIN) + GetKillerAnzahl()))
        return false;

    // Killer Figuren zuteilen
    for (uint8 ui = 0; ui < GetKillerAnzahl() && mSpielerGuidsTemp.size() > 0; ui++)
    {
        SpielerMap::iterator itr = mSpielerGuidsTemp.begin();
        std::advance(itr, urand(0, mSpielerGuidsTemp.size() - 1));

        if (Player* pPlayer = GetSpieler(itr->first))
        {
            // Bei der Camper Action-Figur wird die
            // Lebensanzeige vom Killer um 1 erhoeht
            m_mKiller[itr->first] = Killer((pPlayer->HasItemCount(ITEM_CAMPER_ACTION_FIGUR, 1)) ? GetLebenAnzahl() + 1 : GetLebenAnzahl());
        }

        mSpielerGuidsTemp.erase(itr);
    }

    // Sollte theoretisch nicht passieren,
    // aber sicher ist sicher :?
    if (m_mKiller.size() < GetKillerAnzahl())
        return false;

    // Die restlichen Spieler sind Camper

    // Camper Figuren zuteilen
    for (SpielerMap::const_iterator itr = mSpielerGuidsTemp.begin(); itr != mSpielerGuidsTemp.end(); ++itr)
    {
        if (Player* pPlayer = GetSpieler(itr->first))
        {
            // Bei der Camper Action-Figur wird die
            // Lebensanzeige vom Camper um 1 erhoeht
            m_mCamper[itr->first] = Camper((pPlayer->HasItemCount(ITEM_CAMPER_ACTION_FIGUR, 1)) ? GetLebenAnzahl() + 1 : GetLebenAnzahl());
        }
    }

    // Ohne Camper koennen wir in der Tat nicht starten -.-
    if (m_mCamper.size() == 0)
        return false;

    return true;
}

// Der Spieler mit der Guid "Guid" betritt das Spiel
void WhoIsTheKillerSpiel::SpielBeitreten(ObjectGuid Guid)
{
    m_mSpielerGuids[Guid] = true;

    // Debug
    sLog.outDebug(">> Spiel beigetreten.");
}

// Der Spieler mit der Guid "Guid" verlaesst das Spiel
void WhoIsTheKillerSpiel::SpielVerlassen(ObjectGuid Guid)
{
    if (IstAlsKillerAngemeldet(Guid))
    {
        m_mKiller.erase(Guid);
    }
    else if (IstAlsCamperAngemeldet(Guid))
        m_mCamper.erase(Guid);

    // Wenn der Spieler ein Signalcamper ist
    for (KillerMap::const_iterator itr = m_mKiller.begin(); itr != m_mKiller.end(); ++itr)
    {
        if (itr->second.SignalCamperGuid == Guid)
            m_mKiller[itr->first].SignalCamperGuid.Clear();
    }

    m_mSpielerGuids.erase(Guid);

    // Debug
    sLog.outDebug(">> Spiel verlassen.");
}

// Das Spiel wird beendet
void WhoIsTheKillerSpiel::SpielBeenden()
{
    // Killer
    for (KillerMap::iterator itr = m_mKiller.begin(); itr != m_mKiller.end();)
    {
        Player* pKiller = GetSpieler(itr->first);

        m_mKiller.erase(itr++);

        if (!pKiller)
            continue;

        KillerModifikationenEntfernen(pKiller);
        SpielerTeleportieren(pKiller, WITK_TELEPORT_SPIELERTREFF);
    }

    // Camper
    for (CamperMap::iterator itr = m_mCamper.begin(); itr != m_mCamper.end();)
    {
        Player* pCamper = GetSpieler(itr->first);

        m_mCamper.erase(itr++);

        if (!pCamper)
            continue;

        CamperModifikationenEntfernen(pCamper);
        SpielerTeleportieren(pCamper, WITK_TELEPORT_SPIELERTREFF);
    }

    m_mSpielerGuids.clear();

    SetSpielStatus(STATUS_ENDE);

    // Debug
    sLog.outDebug(">> Spiel beendet.");
}

// Erzeugt die UserInterface Leisten von Who is the Killer
void WhoIsTheKillerSpiel::UserInterface(ObjectGuid Guid, WorldPacket &data, uint32 &count)
{
    if (!IstAlsKillerAngemeldet(Guid) && !IstAlsCamperAngemeldet(Guid))
        return;

    if (GetSpielStatus() > STATUS_COUNTDOWN && GetSpielStatus() < STATUS_SCHLUSS)
    {
        uint32 uiZeitEnde = 0;

        switch (GetSpielStatus())
        {
            case STATUS_KILLEN:
                uiZeitEnde = m_uiStatusKillenEnde;
                break;
            case STATUS_ABSTIMMEN:
                uiZeitEnde = m_uiStatusKillenEnde + 60;
                break;
            case STATUS_FLUCHT:
                uiZeitEnde = m_uiStatusKillenEnde + 60 + m_uiStatusFluchtEnde;
                break;
            default:
                break;
        }

        // Leiste Verbleibende Zeit
        data << uint32(UI_LEISTE_ZEIT);
        data << uint32(1);
        ++count;

        // Anzeige Verbleibende Zeit
        // Minuten werden angezeigt
        data << uint32(UI_ANZEIGE_VERBLEIBENDE_ZEIT_M);
        if ((uiZeitEnde - GetSpielZeit()) > 60)
        {
            data << uint32((uiZeitEnde - GetSpielZeit()) / 60);
        }
        else
            data << uint32(0);

        ++count;

        // Sekunden werden angezeigt
        data << uint32(UI_ANZEIGE_VERBLEIBENDE_ZEIT_S);
        if ((uiZeitEnde - GetSpielZeit()) > 60)
        {
            data << uint32((uiZeitEnde - GetSpielZeit()) % 60);
        }
        else
            data << uint32(uiZeitEnde - GetSpielZeit());

        ++count;
    }

    if (GetSpielStatus() == STATUS_KILLEN)
    {
        // Leiste Verbleibende Leben
        data << uint32(UI_LEISTE_LEBEN);
        data << uint32(1);
        ++count;
        
        // ** Anzeige Verbleibende Leben **

        uint8 uiLeben = 0;

        if (IstAlsKillerAngemeldet(Guid))
        {
            uiLeben = m_mKiller[Guid].uiFakeLeben;
        }
        else if (IstAlsCamperAngemeldet(Guid))
            uiLeben = m_mCamper[Guid].uiLeben;

        data << uint32(UI_ANZEIGE_VERBLEIBENDE_LEBEN);
        data << uint32(uiLeben);
        ++count;
    }

    // Debug
    sLog.outDebug(">> WitK UserInterface Anzeige");
}

// Sendet ein Aktualisierungspacket an einen Spieler, wenn pPlayer nicht gleich NULL ist, ansonsten an alle Mitspieler
void WhoIsTheKillerSpiel::AktualisierungsPacketSenden(uint32 uiFeld, uint32 uiWert, Player* pPlayer)
{
    WorldPacket data;
    data.Initialize(SMSG_UPDATE_WORLD_STATE, 4+4);
    data << uint32(uiFeld);
    data << uint32(uiWert);

    // Das Packet soll nicht gesendet werden, wenn der Spieler z.B. teleportiert wird
    if (pPlayer && !pPlayer->IsInWorld())
        return;

    if (!pPlayer)
    {
        for (KillerMap::const_iterator itr = m_mKiller.begin(); itr != m_mKiller.end(); ++itr)
        {
            if (Player* pKiller = GetSpieler(itr->first))
                if (pKiller->IsInWorld())
                    pKiller->GetSession()->SendPacket(&data);
        }

        for (CamperMap::const_iterator itr = m_mCamper.begin(); itr != m_mCamper.end(); ++itr)
        {
            if (Player* pCamper = GetSpieler(itr->first))
                if (pCamper->IsInWorld())
                    pCamper->GetSession()->SendPacket(&data);
        }
    }
    else
        pPlayer->GetSession()->SendPacket(&data);
}

// Die Verbleibende Zeit wird fuer einen Spieler aktualisiert
void WhoIsTheKillerSpiel::VerbleibendeZeitAktualisieren(Player* pPlayer)
{
    uint32 uiEnde = 0;
    uint32 uiDiff = 0;
    
    switch (GetSpielStatus())
    {
        case STATUS_KILLEN:
            uiEnde = m_uiStatusKillenEnde;
            break;
        case STATUS_ABSTIMMEN:
            uiEnde = m_uiStatusKillenEnde + 60;
            break;
        case STATUS_FLUCHT:
            uiEnde = m_uiStatusKillenEnde + 60 + m_uiStatusFluchtEnde;
            break;
        default:
            return;
    }

    uiDiff = uiEnde - GetSpielZeit();

    if (uiDiff > 60)
    {
        AktualisierungsPacketSenden(UI_ANZEIGE_VERBLEIBENDE_ZEIT_M, uiDiff / 60, pPlayer);
        AktualisierungsPacketSenden(UI_ANZEIGE_VERBLEIBENDE_ZEIT_S, uiDiff % 60, pPlayer);
    }
    else
    {
        AktualisierungsPacketSenden(UI_ANZEIGE_VERBLEIBENDE_ZEIT_M, 0, pPlayer);
        AktualisierungsPacketSenden(UI_ANZEIGE_VERBLEIBENDE_ZEIT_S, uiDiff, pPlayer);
    }
}

// Meldet Afk Spieler ab
void WhoIsTheKillerSpiel::AFK(Player* pPlayer)
{
    if (!pPlayer)
        return;

    // Spieler die Afk sind, werden erst ab STATUS_KILLEN abgemeldet
    if (GetSpielStatus() < STATUS_KILLEN)
        return;

    if (IstAlsKillerAngemeldet(pPlayer->GetObjectGuid()))
    {
        KillerModifikationenEntfernen(pPlayer);
    }
    else if (IstAlsCamperAngemeldet(pPlayer->GetObjectGuid()))
        CamperModifikationenEntfernen(pPlayer);

    // Spieler abmelden
    SpielVerlassen(pPlayer->GetObjectGuid());

    // Der Spieler wird in seinen Spielertreff teleportiert
    SpielerTeleportieren(pPlayer, WITK_TELEPORT_SPIELERTREFF);
}

// Wird aufgerufen, wenn ein angemeldeter Spieler die Zone verlassen hat
// Wichtig: In dieser Funktion darf der Spieler nicht teleportiert werden ( ! )
void WhoIsTheKillerSpiel::ZonenWechsel(Player* pPlayer)
{
    if (!pPlayer)
        return;

    // Nur in STATUS_KILLEN
    if (GetSpielStatus() < STATUS_KILLEN)
        return;

    // Erst 10 Sekunden nach Spielstart
    if (GetSpielZeit() <= 10)
        return;

    // Wenn der Spieler nicht als Killer und Camper angemeldet ist ( ! )
    if (!IstAlsKillerAngemeldet(pPlayer->GetObjectGuid()) && !IstAlsCamperAngemeldet(pPlayer->GetObjectGuid()))
        return;

    if (GetSpielStatus() == STATUS_SCHLUSS)
    {
        SpielVerlassen(pPlayer->GetObjectGuid());
    }
    else if (pPlayer->GetZoneId() != 10)
    {
        if (IstAlsKillerAngemeldet(pPlayer->GetObjectGuid()))
        {
            m_mKiller[pPlayer->GetObjectGuid()].uiZonenTimer = sWhoIsTheKillerMgr.GetEinstellung(ZONEN_T_WECHSEL);
        }
        else if (IstAlsCamperAngemeldet(pPlayer->GetObjectGuid()))
            m_mCamper[pPlayer->GetObjectGuid()].uiZonenTimer = sWhoIsTheKillerMgr.GetEinstellung(ZONEN_T_WECHSEL);

        char cNachricht[100];
        sprintf(cNachricht, GetNachricht(WITK_TEXTID_SPIELZONE_VERLASSEN), uint16(sWhoIsTheKillerMgr.GetEinstellung(ZONEN_T_WECHSEL)));
        NachrichtSenden(cNachricht, false, true, pPlayer);
    }

    // Debug
    sLog.outDebug(">> Spielzone verlassen.");
}

// Wird aufgerufen, wenn ein angemeldeter Spieler die AreaId wechselt
void WhoIsTheKillerSpiel::AreaWechsel(Player* pPlayer, uint32 uiAreaId)
{
    // AreaId 10 (Daemmerwald) ist zu gross
    // das Abspielen der Musik wuerde sich nicht lohnen
    if (uiAreaId == 10)
        return;

    // Erst 10 Sekunden nach Spielstart
    if (GetSpielZeit() <= 10)
        return;

    // Nur in STATUS_KILLEN
    if (GetSpielStatus() != STATUS_KILLEN)
        return;
    
    // Der Sound soll nur abgespielt werden, wenn der Killer das Areal betritt
    if (!IstAlsKillerAngemeldet(pPlayer->GetObjectGuid()))
        return;

    bool bCamperImAreal = false;

    for (CamperMap::const_iterator itr = m_mCamper.begin(); itr != m_mCamper.end(); ++itr)
    {
        if (Player* pCamper = GetSpieler(itr->first))
        {
            // In dem Fall ist keine Warnung moeglich
            if (!pCamper->IsInWorld() || pCamper->IsBeingTeleported())
                continue;

            // Wenn der Camper im selben Areal wie der Killer ist
            if (pCamper->GetAreaId() == uiAreaId)
            {
                pCamper->PlayDirectSound(SOUND_WARNUNG, pCamper);
                bCamperImAreal = true;
            }
        }
    }

    if (!pPlayer)
        return;

    // Wenn mindestens ein Camper im Areal ist,
    // wird die Musik auch beim Killer abgespielt
    if (bCamperImAreal)
        pPlayer->PlayDirectSound(SOUND_WARNUNG, pPlayer);
}

// Wird aufgerufen wenn ein Killer gekillt wurde
void WhoIsTheKillerSpiel::KillerTot(Player* pKiller)
{
    if (!pKiller)
        return;

    if (GetSpielStatus() == STATUS_FLUCHT)
    {
        // Modifikationen werden entfernt
        KillerModifikationenEntfernen(pKiller);
        // Nachricht... Killer tot verloren
        NachrichtSenden(WITK_TEXTID_KILLER_BESIEGT_KILLER, true, true, pKiller);
        // Der Killer bekommt eine Belohnung
        SpielerBelohnung(pKiller, STATUS_VERLOREN);
        // Der Killer verleasst das Spiel
        SpielVerlassen(pKiller->GetObjectGuid());
        // Der Killer wird in seinen Spielertreff teleportiert
        SpielerTeleportieren(pKiller, WITK_TELEPORT_SPIELERTREFF);
        
        // Wenn kein Killer mehr angemeldet ist
        if (!m_mKiller.size())
        {
            for (CamperMap::const_iterator itr = m_mCamper.begin(); itr != m_mCamper.end(); ++itr)
            {
                if (Player* pCamper = GetSpieler(itr->first))
                {
                    CamperModifikationenEntfernen(pCamper);
                    NachrichtSenden(WITK_TEXTID_KILLER_BESIEGT_CAMPER, true, true, pCamper);
                    SpielerBelohnung(pCamper, STATUS_GEWONNEN);
                }
            }

            SetSpielStatus(STATUS_SCHLUSS);
        }
    }

    // Debug
    sLog.outDebug(">> %s wurde gekillt.", pKiller->GetName());
}

// Wird aufgerufen wenn ein Camper gekillt wurde
void WhoIsTheKillerSpiel::CamperTot(Player* pCamper)
{
    if (!pCamper)
        return;

    if (GetSpielStatus() >= STATUS_KILLEN && GetSpielStatus() <= STATUS_FLUCHT)
        m_mCamper[pCamper->GetObjectGuid()].uiWiederbelebungsTimer = sWhoIsTheKillerMgr.GetEinstellung(CAMPER_WBLT);

    // Debug
    sLog.outDebug(">> %s wurde gekillt.", pCamper->GetName());
}

// Wird aufgerufen, wenn der Killer auf das Portal klickt
void WhoIsTheKillerSpiel::KillerPortal(Player* pKiller)
{
    if (!pKiller)
        return;

    // Modifikationen werden entfernt
    KillerModifikationenEntfernen(pKiller);
    // Nachricht... Killer durch Portal entkommen gewonnen
    NachrichtSenden(WITK_TEXTID_PORTAL_ENTKOMMEN_KILLER, true, true, pKiller);
    // Der Killer bekommt eine Belohnung
    SpielerBelohnung(pKiller, STATUS_GEWONNEN);
    // Der Killer wird abgemeldet
    SpielVerlassen(pKiller->GetObjectGuid());
    // Der Killer wird in seinen Spielertreff teleportiert
    SpielerTeleportieren(pKiller, WITK_TELEPORT_SPIELERTREFF);

    // Die restlichen Killer... haben verloren
    for (KillerMap::const_iterator itr = m_mKiller.begin(); itr != m_mKiller.end(); ++itr)
    {
        if (Player* pKiller = GetSpieler(itr->first))
        {
            CamperModifikationenEntfernen(pKiller);
            NachrichtSenden(WITK_TEXTID_KILLER_VOR_DIR_DURCH_PORTAL, true, true, pKiller);
            SpielerBelohnung(pKiller, STATUS_VERLOREN);
        }
    }

    // Die Camper haben verloren
    for (CamperMap::const_iterator itr = m_mCamper.begin(); itr != m_mCamper.end(); ++itr)
    {
        if (Player* pCamper = GetSpieler(itr->first))
        {
            CamperModifikationenEntfernen(pCamper);
            NachrichtSenden(WITK_TEXTID_PORTAL_ENTKOMMEN_CAMPER, true, true, pCamper);
            SpielerBelohnung(pCamper, STATUS_VERLOREN);
        }
    }

    SetSpielStatus(STATUS_SCHLUSS);
}

// Der Killer wechselt die Figur
bool WhoIsTheKillerSpiel::FigurWechseln(Player* pKiller)
{
    if (!pKiller)
        return false;

    // Der Spieler ist kein Killer
    if (!IstAlsKillerAngemeldet(pKiller->GetObjectGuid()))
    {
        NachrichtSenden(WITK_TEXTID_KANN_NICHT_VERWENDET_WERDEN, false, true, pKiller);
        return false;
    }

    // SpielStatus ist nicht auf STATUS_KILLEN
    if (GetSpielStatus() != STATUS_KILLEN)
    {
        NachrichtSenden(WITK_TEXTID_KANN_NICHT_VERWENDET_WERDEN, false, true, pKiller);
        return false;
    }

    // Wenn der Spieler bereits die Killer Figur spielt
    // wird er in einen Camper verwandelt
    if (m_mKiller[pKiller->GetObjectGuid()].bKillerFigur)
    {
        m_mKiller[pKiller->GetObjectGuid()].bKillerFigur = false;
        KillerModifikationenEntfernen(pKiller);
        CamperModifikationenSetzen(pKiller);
    }
    // Sonst wird er wieder in den Killer verwandelt
    else
    {
        m_mKiller[pKiller->GetObjectGuid()].bKillerFigur = true;
        CamperModifikationenEntfernen(pKiller);
        KillerModifikationenSetzen(pKiller);
    }

    // Syntax und Beschreibung werden nicht angezeigt
    return true;
}

// Der vom Killer angeklickte Camper wird gekillt
bool WhoIsTheKillerSpiel::CamperKillen(Player* pKiller, Player* pCamper)
{
    if (!pKiller || !pCamper)
        return false;

    // Spieler ist nicht der Killer
    if (!IstAlsKillerAngemeldet(pKiller->GetObjectGuid()))
    {
        NachrichtSenden(WITK_TEXTID_KANN_NICHT_VERWENDET_WERDEN, false, true, pKiller);
        return false;
    }

    // SpielStatus ist nicht auf STATUS_KILLEN
    if (GetSpielStatus() != STATUS_KILLEN)
    {
        NachrichtSenden(WITK_TEXTID_KANN_NICHT_VERWENDET_WERDEN, false, true, pKiller);
        return false;
    }

    // Falsche Figur
    if (!m_mKiller[pKiller->GetObjectGuid()].bKillerFigur)
    {
        NachrichtSenden(WITK_TEXTID_BRAUCHT_KILLER_FIGUR, false, true, pKiller);
        return false;
    }

    // Killer hat nichts oder sich selbst angeklickt
    if (pKiller->GetObjectGuid() == pCamper->GetObjectGuid())
    {
        NachrichtSenden(WITK_TEXTID_ZUERST_CAMPER_ANKLICKEN, false, true, pKiller);
        return true; // Syntax und Beschreibung werden nicht angezeigt
    }

    // Angeklickter Spieler ist in diesem Spiel nicht als Camper angemeldet
    if (!IstAlsCamperAngemeldet(pCamper->GetObjectGuid()))
    {
        NachrichtSenden(WITK_TEXTID_SPIELER_IST_KEIN_CAMPER, false, true, pKiller);
        return true; // Syntax und Beschreibung werden nicht angezeigt
    }

    // Camper ist zu weit entfernt
    if (pKiller->GetDistance(pCamper) > sWhoIsTheKillerMgr.GetEinstellung(AKTIONS_ENTFERNUNG_MAX))
    {
        NachrichtSenden(WITK_TEXTID_CAMPER_ZU_WEIT_ENTFERNT, false, true, pKiller);
        return true; // Syntax und Beschreibung werden nicht angezeigt
    }

    // Camper wurde noch nicht gefangen
    if (!pCamper->HasAura(SPELL_SCHLAF, EFFECT_INDEX_0) && !pCamper->HasAura(SPELL_NETZ, EFFECT_INDEX_0))
    {
        NachrichtSenden(WITK_TEXTID_CAMPER_MUSS_ZUERST_GEFANGEN_WERDEN, false, true, pKiller);
        return true; // Syntax und Beschreibung werden nicht angezeigt
    }

    // Der Killer hat den Camper gerade erst gefangen und muss noch ein bisschen warten
    if (m_mKiller[pKiller->GetObjectGuid()].uiCooldownKillen > 0)
    {
        // Nachricht wird in der Update() Funktion gesendet
        return true; // Syntax und Beschreibung werden nicht angezeigt
    }

    // Der Camper ist der SignalCamper vom Killer
    if (m_mKiller[pKiller->GetObjectGuid()].SignalCamperGuid == pCamper->GetObjectGuid())
    {
        SignalCamperWechseln(pKiller->GetObjectGuid());
        SignalCamperSenden(pKiller, true);
    }

    if (m_mCamper[pCamper->GetObjectGuid()].uiLeben > 0)
    {
        // Dem Camper wird 1 Leben abgezogen
        m_mCamper[pCamper->GetObjectGuid()].uiLeben--;
        // Verbleibende Leben wird aktualisiert
        AktualisierungsPacketSenden(UI_ANZEIGE_VERBLEIBENDE_LEBEN, m_mCamper[pCamper->GetObjectGuid()].uiLeben, pCamper);
    }

    char cNachricht[100];

    if (m_mCamper[pCamper->GetObjectGuid()].uiLeben > 0)
    {
        sprintf(cNachricht, GetNachricht(WITK_TEXTID_SPIELER_GEKILLT), pCamper->GetName());
        NachrichtSenden(cNachricht, false, true, NULL);
        // Camper killt "sich selber", damit der Name des Killers im Kampflog nicht angezeigt wird
        pCamper->DealDamage(pCamper, pCamper->GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
        // Spieler wird tot an einen zufaelligen neuen Platz teleportiert
        SpielerTeleportieren(pCamper, urand(0, WITK_TELEPORT_ZUFALL_MAX));
    }
    else
    {
        // Modifikationen werden entfernt
        CamperModifikationenEntfernen(pCamper);
        // Nachricht
        sprintf(cNachricht, GetNachricht(WITK_TEXTID_SPIELER_BESIEGT), pCamper->GetName());
        NachrichtSenden(cNachricht, false, true, NULL);
        NachrichtSenden(WITK_TEXTID_CAMPER_BESIEGT_CAMPER, true, false, pCamper);
        // Der Camper wird abgemeldet
        SpielVerlassen(pCamper->GetObjectGuid());
        // Der Camper wird in seinen Spielertreff teleportiert
        SpielerTeleportieren(pCamper, WITK_TELEPORT_SPIELERTREFF);
    }

    // Syntax und Beschreibung werden nicht angezeigt
    return true;
}

// Der Camper wird gefangen, damit er gekillt werden kann
bool WhoIsTheKillerSpiel::CamperFangen(Player* pKiller, Player* pCamper)
{
    if (!pKiller || !pCamper)
        return false;

    // Spieler ist nicht der Killer
    if (!IstAlsKillerAngemeldet(pKiller->GetObjectGuid()))
    {
        NachrichtSenden(WITK_TEXTID_KANN_NICHT_VERWENDET_WERDEN, false, true, pKiller);
        return false;
    }

    // SpielStatus ist nicht auf STATUS_KILLEN
    if (GetSpielStatus() != STATUS_KILLEN)
    {
        NachrichtSenden(WITK_TEXTID_KANN_NICHT_VERWENDET_WERDEN, false, true, pKiller);
        return false;
    }

    // Falsche Figur
    if (!m_mKiller[pKiller->GetObjectGuid()].bKillerFigur)
    {
        NachrichtSenden(WITK_TEXTID_BRAUCHT_KILLER_FIGUR, false, true, pKiller);
        return false;
    }

    // Killer hat nichts oder sich selbst angeklickt
    if (pKiller->GetObjectGuid() == pCamper->GetObjectGuid())
    {
        NachrichtSenden(WITK_TEXTID_ZUERST_CAMPER_ANKLICKEN, false, true, pKiller);
        return true; // Syntax und Beschreibung werden nicht angezeigt
    }

    // Angeklickter Spieler ist nicht als Camper angemeldet
    if (!IstAlsCamperAngemeldet(pCamper->GetObjectGuid()))
    {
        NachrichtSenden(WITK_TEXTID_SPIELER_IST_KEIN_CAMPER, false, true, pKiller);
        return true; // Syntax und Beschreibung werden nicht angezeigt
    }

    // Camper ist zu weit entfernt
    if (pKiller->GetDistance(pCamper) > sWhoIsTheKillerMgr.GetEinstellung(AKTIONS_ENTFERNUNG_MAX))
    {
        NachrichtSenden(WITK_TEXTID_CAMPER_ZU_WEIT_ENTFERNT, false, true, pKiller);
        return true; // Syntax und Beschreibung werden nicht angezeigt
    }

    // Camper wurde bereits gefangen
    if (pCamper->HasAura(SPELL_SCHLAF, EFFECT_INDEX_0) || pCamper->HasAura(SPELL_NETZ, EFFECT_INDEX_0))
    {
        NachrichtSenden(WITK_TEXTID_CAMPER_WURDE_BEREITS_GEFANGEN, false, true, pKiller);
        return true; // Syntax und Beschreibung werden nicht angezeigt
    }

    // Es kann alle "uiCooldownKillen" Sekunden ein neuer Camper gefangen werden
    if (m_mKiller[pKiller->GetObjectGuid()].uiCooldownKillen > 0)
    {
        NachrichtSenden(WITK_TEXTID_BEREITS_ANDEREN_CAMPER_GEFANGEN, false, true, pKiller);
        return true; // Syntax und Beschreibung werden nicht angezeigt
    }

    uint32 uiSpellId = 0;

    // Camper wird gefangen
    if (rand() % 2)
    {
        uiSpellId = SPELL_NETZ;
    }
    else
        uiSpellId = SPELL_SCHLAF;

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(uiSpellId);

    if (!spellInfo)
        return true;

    // Wenn der Spell keine Aura hat (Kann nur passieren wenn ich die falschen SpellIds eintrage (?) )
    if (!IsSpellAppliesAura(spellInfo) && !IsSpellHaveEffect(spellInfo, SPELL_EFFECT_PERSISTENT_AREA_AURA))
        return true;

    SpellAuraHolder* holder = CreateSpellAuraHolder(spellInfo, pCamper, pCamper);

    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        uint8 eff = spellInfo->Effect[i];

        if (eff >= TOTAL_SPELL_EFFECTS)
            continue;

        if (IsAreaAuraEffect(eff) || eff == SPELL_EFFECT_APPLY_AURA || eff == SPELL_EFFECT_PERSISTENT_AREA_AURA)
        {
            Aura* aur = CreateAura(spellInfo, SpellEffectIndex(i), NULL, holder, pCamper);
            holder->AddAura(aur, SpellEffectIndex(i));
        }
    }

    pCamper->AddSpellAuraHolder(holder);

    // Cooldown wird gesetzt
    m_mKiller[pKiller->GetObjectGuid()].uiCooldownKillen = sWhoIsTheKillerMgr.GetEinstellung(KILLER_CD_KILLEN);

    return true; // Syntax und Beschreibung werden nicht angezeigt
}

// Schickt eine Fake Nachricht an alle Spieler
bool WhoIsTheKillerSpiel::CamperVerwirren(Player* pKiller)
{
    if (!pKiller)
        return false;

    // Spieler ist nicht der Killer
    if (!IstAlsKillerAngemeldet(pKiller->GetObjectGuid()))
    {
        NachrichtSenden(WITK_TEXTID_KANN_NICHT_VERWENDET_WERDEN, false, true, pKiller);
        return false;
    }

    // SpielStatus ist nicht auf STATUS_KILLEN
    if (GetSpielStatus() != STATUS_KILLEN)
    {
        NachrichtSenden(WITK_TEXTID_KANN_NICHT_VERWENDET_WERDEN, false, true, pKiller);
        return false;
    }

    // Falsche Figur
    if (!m_mKiller[pKiller->GetObjectGuid()].bKillerFigur)
    {
        NachrichtSenden(WITK_TEXTID_BRAUCHT_KILLER_FIGUR, false, true, pKiller);
        return false;
    }

    // Der Killer hat keine Fakeleben mehr
    if (m_mKiller[pKiller->GetObjectGuid()].uiFakeLeben == 0)
    {
        NachrichtSenden(WITK_TEXTID_FAKE_CAMPER_BEREITS_BESIEGT, false, true, pKiller);
        return true; // Syntax und Beschreibung werden nicht angezeigt
    }
    else if (m_mKiller[pKiller->GetObjectGuid()].uiFakeLeben > 0)
    {
        // Dem Killer wird 1 Fakeleben abgezogen
        m_mKiller[pKiller->GetObjectGuid()].uiFakeLeben--;
        // Verbleibende Leben wird aktualisiert
        AktualisierungsPacketSenden(UI_ANZEIGE_VERBLEIBENDE_LEBEN, m_mKiller[pKiller->GetObjectGuid()].uiFakeLeben, pKiller);
    }

    char cNachricht[100];

    // Wenn der Killer jetzt noch Fakeleben hat
    if (m_mKiller[pKiller->GetObjectGuid()].uiFakeLeben > 0)
    {
        sprintf(cNachricht, GetNachricht(WITK_TEXTID_SPIELER_GEKILLT), pKiller->GetName());
    }
    // Wenn er jetzt keine mehr hat ist sein Fakecamper besiegt
    else
    {
        sprintf(cNachricht, GetNachricht(WITK_TEXTID_SPIELER_BESIEGT), pKiller->GetName());
    }

    NachrichtSenden(cNachricht, false, true, NULL);

    return true; // Syntax und Beschreibung werden nicht angezeigt
}

// Wechselt das Camper Signal vom Killer
bool WhoIsTheKillerSpiel::SignalWechseln(Player* pKiller)
{
    if (!pKiller)
        return false;

    // Spieler ist nicht der Killer
    if (!IstAlsKillerAngemeldet(pKiller->GetObjectGuid()))
    {
        NachrichtSenden(WITK_TEXTID_KANN_NICHT_VERWENDET_WERDEN, false, true, pKiller);
        return false;
    }

    // SpielStatus ist nicht auf STATUS_KILLEN
    if (GetSpielStatus() != STATUS_KILLEN)
    {
        NachrichtSenden(WITK_TEXTID_KANN_NICHT_VERWENDET_WERDEN, false, true, pKiller);
        return false;
    }

    // Falsche Figur
    if (!m_mKiller[pKiller->GetObjectGuid()].bKillerFigur)
    {
        NachrichtSenden(WITK_TEXTID_BRAUCHT_KILLER_FIGUR, false, true, pKiller);
        return false;
    }

    SignalCamperWechseln(pKiller->GetObjectGuid());
    SignalCamperSenden(pKiller, true);

    return true; // Syntax und Beschreibung werden nicht angezeigt
}

// Mit dem TextId Parameter...
void WhoIsTheKillerSpiel::NachrichtSenden(int32 iTextId, bool bChat, bool bScreen, Player* pPlayer, bool bKiller)
{
    NachrichtSenden(GetNachricht(iTextId), bChat, bScreen, pPlayer, bKiller);
}

// Sendet die Nachricht "sNachricht" an den Spieler "pPlayer" wenn er ungleich NULL ist.
// Ist "pPlayer" gleich NULL, wird die Nachricht an alle Spieler gesendet.
// Wenn bKiller "false" ist, wird die Nachricht - wenn sie an alle gesendet werden soll - nicht an den Killer gesendet.
void WhoIsTheKillerSpiel::NachrichtSenden(const char* cNachricht, bool bChat, bool bScreen, Player* pPlayer, bool bKiller)
{
    if (pPlayer)
    {
        // Die Nachricht kann nicht gesendet werden...
        if (!pPlayer->IsInWorld() || pPlayer->IsBeingTeleported())
            return;

        if (bChat)
        {
            WorldPacket data(SMSG_SERVER_MESSAGE, 50);
            data << uint32(3);
            data << cNachricht;
            pPlayer->GetSession()->SendPacket(&data);
        }

        if (bScreen)
            pPlayer->GetSession()->SendAreaTriggerMessage(cNachricht);
    }
    else
    {
        if (bKiller)
        {
            for (KillerMap::const_iterator itr = m_mKiller.begin(); itr != m_mKiller.end(); ++itr)
            {
                if (Player* pKiller = GetSpieler(itr->first))
                    NachrichtSenden(cNachricht, bChat, bScreen, pKiller);
            }
        }

        for (CamperMap::const_iterator itr = m_mCamper.begin(); itr != m_mCamper.end(); ++itr)
        {
            if (Player* pCamper = GetSpieler(itr->first))
                NachrichtSenden(cNachricht, bChat, bScreen, pCamper);
        }
    }
}

// Schickt den Spieler mit der Guid "Guid" zurueck
Player* WhoIsTheKillerSpiel::GetSpieler(ObjectGuid Guid)
{
    return HashMapHolder<Player>::Find(Guid);;
}

// Schickt die Nachricht mit der Id "iTextId" zurueck
const char* WhoIsTheKillerSpiel::GetNachricht(int32 iTextId)
{
    return (iTextId) ? sObjectMgr.GetMangosString(iTextId, LOCALE_deDE) : "";
}

// Erstellt fuer den Spieler "pPlayer" eine Liste mit den Spielern vom Spiel
void WhoIsTheKillerSpiel::ErstelleSpielerListe(Player* pPlayer, uint8 uiIconId, uint32 uiSender)
{
    uint8 uiNummer = 1;

    char cAnzeige[100];

    for (SpielerMap::iterator itr = m_mSpielerGuids.begin(); itr != m_mSpielerGuids.end();)
    {
        if (Player* pUser = GetSpieler(itr->first))
        {
            // Wenn dieser Spieler der Leiter oder ein GM ist, kann er Spieler aus dem Spiel kicken
            if ((GetLeiterGuid() == pPlayer->GetObjectGuid() || pPlayer->GetSession()->GetSecurity() >= SEC_GAMEMASTER)
            // Der Leiter darf nicht gekickt werden
            && GetLeiterGuid() != pUser->GetObjectGuid()
            // Ein GM darf auch nicht gekickt werden
            && pUser->GetSession()->GetSecurity() < SEC_GAMEMASTER
            // Spieler koennen nicht mehr gekickt werden, wenn das Spiel bereits gestartet wurde
            && GetSpielStatus() == STATUS_WARTEN)
            {
                sprintf(cAnzeige, GetNachricht(WITK_TEXTID_SPIELER_KICKEN), uiNummer, pUser->GetName());
                pPlayer->PlayerTalkClass->GetGossipMenu().AddMenuItem(uiIconId, cAnzeige, uiSender, pUser->GetObjectGuid().GetRawValue(), "", 0);
            }
            else
            {
                sprintf(cAnzeige, GetNachricht(WITK_TEXTID_SPIELER_NORMAL), uiNummer, pUser->GetName());
                pPlayer->PlayerTalkClass->GetGossipMenu().AddMenuItem(uiIconId, cAnzeige, uiSender, 0, "", 0);
            }

            uiNummer++;

            ++itr;
        }
        // ...sonst wird der Spieler abgemeldet, der Leiter nicht (!)
        else if (itr->first != GetLeiterGuid())
            m_mSpielerGuids.erase(itr++);
    }
}

// Erstellt fuer den Spieler "pPlayer" eine Abstimmungsliste
void WhoIsTheKillerSpiel::ErstelleAbstimmungsListe(Player* pPlayer, uint8 uiIconId, uint32 uiSender)
{
    for (SpielerMap::const_iterator itr = m_mSpielerGuids.begin(); itr != m_mSpielerGuids.end(); ++itr)
    {
        // Der eigene Name wird nicht angezeigt
        if (pPlayer->GetObjectGuid() != itr->first)
            if (Player* pUser = GetSpieler(itr->first))
                pPlayer->PlayerTalkClass->GetGossipMenu().AddMenuItem(uiIconId, pUser->GetName(), uiSender, pUser->GetObjectGuid().GetRawValue(), "", 0);
    }
}

// Wird jede Sekunde aufgerufen, wenn das Spiel gestartet wurde
void WhoIsTheKillerSpiel::SpielGeschehen()
{
    // In den Schleifen koennen Spieler in verschiedenen Funktionen abgemeldet werden
    KillerMap TempKillerMap = m_mKiller;

    // Killer
    for (KillerMap::const_iterator itr = TempKillerMap.begin(); itr != TempKillerMap.end(); ++itr)
    {
        SpielGeschehenKiller(itr->first);

        Player* pKiller = GetSpieler(itr->first);

        // Wenn dieser Killer Offline ist
        if (!pKiller)
        {
            // Offline Teleport
            OfflineTeleport(itr->first);
            // Aus der Spielermap entfernen
            m_mSpielerGuids.erase(itr->first);
            // Aus der Killermap entfernen und den Iterator aktualisieren
            m_mKiller.erase(itr->first);
        }
        else if (pKiller->IsInWorld() && !pKiller->IsBeingTeleported())
        {
            VerbleibendeZeitAktualisieren(pKiller);

            // Zonentimer
            if (m_mKiller[itr->first].uiZonenTimer > 0)
            {
                m_mKiller[itr->first].uiZonenTimer--;

                if (m_mKiller[itr->first].uiZonenTimer == 0 && pKiller->GetZoneId() != 10)
                {
                    if (GetSpielStatus() == STATUS_ABSTIMMEN)
                    {
                        SpielerTeleportieren(pKiller, WITK_TELEPORT_WEGKREUZUNG);
                    }
                    else
                        SpielerTeleportieren(pKiller, WITK_TELEPORT_KILLER_START);
                }
            }

            // Befehl ".killen" Cooldown
            if (m_mKiller[itr->first].uiCooldownKillen > 0)
            {
                m_mKiller[itr->first].uiCooldownKillen--;

                if (m_mKiller[itr->first].uiCooldownKillen == 1)
                {
                    NachrichtSenden(WITK_TEXTID_COOLDOWN_KILLEN_1, false, true, pKiller);
                }
                else if (m_mKiller[itr->first].uiCooldownKillen == 0)
                {
                    NachrichtSenden(WITK_TEXTID_COOLDOWN_VORBEI, false, true, pKiller);
                }
                else
                {
                    char cNachricht[100];
                    sprintf(cNachricht, GetNachricht(WITK_TEXTID_COOLDOWN_KILLEN_X), m_mKiller[itr->first].uiCooldownKillen);
                    NachrichtSenden(cNachricht, false, true, pKiller);
                }
            }

            KillerThreadsAbarbeiten(pKiller);
        }
    }

    // In den Schleifen koennen Spieler in verschiedenen Funktionen abgemeldet werden
    CamperMap TempCamperMap = m_mCamper;

    for (CamperMap::const_iterator itr = TempCamperMap.begin(); itr != TempCamperMap.end(); ++itr)
    {
        SpielGeschehenCamper(itr->first);

        Player* pCamper = GetSpieler(itr->first);

        // Wenn dieser Camper Offline ist
        if (!pCamper)
        {
            // Offline Teleport
            OfflineTeleport(itr->first);
            // Aus der Spielermap entfernen
            m_mSpielerGuids.erase(itr->first);
            // Aus der Campermap entfernen und den Iterator aktualisieren
            m_mCamper.erase(itr->first);
        }
        else if (pCamper->IsInWorld() && !pCamper->IsBeingTeleported())
        {
            VerbleibendeZeitAktualisieren(pCamper);

            // Wiederbelebungstimer
            if (m_mCamper[itr->first].uiWiederbelebungsTimer > 0)
            {
                m_mCamper[itr->first].uiWiederbelebungsTimer--;

                if (m_mCamper[itr->first].uiWiederbelebungsTimer == 0)
                    SpielerWiederbeleben(pCamper);
            }

            // Zonentimer
            if (m_mCamper[itr->first].uiZonenTimer > 0)
            {
                m_mCamper[itr->first].uiZonenTimer--;

                if (m_mCamper[itr->first].uiZonenTimer == 0 && pCamper->GetZoneId() != 10)
                {
                    if (GetSpielStatus() == STATUS_ABSTIMMEN)
                    {
                        SpielerTeleportieren(pCamper, WITK_TELEPORT_WEGKREUZUNG);
                    }
                    else
                        SpielerTeleportieren(pCamper, urand(0, WITK_TELEPORT_ZUFALL_MAX));
                }
            }

            CamperThreadsAbarbeiten(pCamper);
        }
    }

    // Wenn alle Killer ausgeschieden sind...
    if (m_mKiller.size() == 0)
    {
        for (CamperMap::const_iterator itr = m_mCamper.begin(); itr != m_mCamper.end(); ++itr)
        {
            if (Player* pCamper = GetSpieler(itr->first))
            {
                // Modifikationen werden entfernt
                CamperModifikationenEntfernen(pCamper);
                // Nachricht... Killer offline
                NachrichtSenden(WITK_TEXTID_KILLER_OFFLINE, true, true, pCamper);
            }
        }

        SetSpielStatus(STATUS_SCHLUSS);
    }
    // Wenn alle Camper ausgeschieden sind...
    else if (m_mCamper.size() == 0)
    {
        for (KillerMap::const_iterator itr = m_mKiller.begin(); itr != m_mKiller.end(); ++itr)
        {
            if (Player* pKiller = GetSpieler(itr->first))
            {
                // Modifikationen werden entfernt
                KillerModifikationenEntfernen(pKiller);
                // Nachricht... Camper offline
                NachrichtSenden(WITK_TEXTID_ALLE_CAMPER_OFFLINE, true, true, pKiller);
            }
        }

        SetSpielStatus(STATUS_SCHLUSS);
    }
}

// Spielgeschehen fuer den Killer
void WhoIsTheKillerSpiel::SpielGeschehenKiller(ObjectGuid Guid)
{
    switch (GetSpielZeit())
    {
        case 0:
            m_mKiller[Guid].vAktionThreads.push_back(0);
            break;
        case 1:
            m_mKiller[Guid].vAktionThreads.push_back(1);
            break;
        case 20:
            m_mKiller[Guid].vAktionThreads.push_back(2);
            break;
        case 30:
            m_mKiller[Guid].vAktionThreads.push_back(3);
            break;
        case 40:
            m_mKiller[Guid].vAktionThreads.push_back(4);
            break;
        case 50:
            m_mKiller[Guid].vAktionThreads.push_back(5);
            break;
    }

    if (GetSpielZeit() == m_uiStatusKillenEnde)
        m_mKiller[Guid].vAktionThreads.push_back(6);

    if (GetSpielZeit() == (m_uiStatusKillenEnde + 60))
        m_mKiller[Guid].vAktionThreads.push_back(7);
}

// Spielgeschehen fuer den Camper
void WhoIsTheKillerSpiel::SpielGeschehenCamper(ObjectGuid Guid)
{
    switch (GetSpielZeit())
    {
        case 0:
            m_mCamper[Guid].vAktionThreads.push_back(128);
            break;
        case 1:
            m_mCamper[Guid].vAktionThreads.push_back(129);
            break;
        case 10:
            m_mCamper[Guid].vAktionThreads.push_back(130);
            break;
        case 20:
            m_mCamper[Guid].vAktionThreads.push_back(131);
            break;
    }

    if (GetSpielZeit() == m_uiStatusKillenEnde)
        m_mCamper[Guid].vAktionThreads.push_back(132);

    if (GetSpielZeit() == (m_uiStatusKillenEnde + 60))
        m_mCamper[Guid].vAktionThreads.push_back(133);
}

// Verarbeitet die gesetzten Aktionen von einem Killer
void WhoIsTheKillerSpiel::KillerThreadsAbarbeiten(Player* pKiller)
{
    if (!pKiller)
        return;

    // Killer Aktion-Threads
    for (std::vector<uint8>::iterator itr = m_mKiller[pKiller->GetObjectGuid()].vAktionThreads.begin(); itr != m_mKiller[pKiller->GetObjectGuid()].vAktionThreads.end(); itr = m_mKiller[pKiller->GetObjectGuid()].vAktionThreads.erase(itr))
    {
        switch (*itr)
        {
            case 0:
                // Killer wird wiederbelebt falls er tot ist
                SpielerWiederbeleben(pKiller);
                // Killer steigt vom Mount
                if (pKiller->IsMounted())
                {
                    pKiller->Unmount();
                    pKiller->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);
                }
                // Killer wird zum Daemmerwald teleportiert
                SpielerTeleportieren(pKiller, WITK_TELEPORT_KILLER_START);
                break;
            case 1:
                KillerModifikationenSetzen(pKiller);
                NachrichtSenden(WITK_TEXTID_KILLER_INFO1, false, true, pKiller);
                break;
            case 2:
                NachrichtSenden(WITK_TEXTID_KILLER_INFO2, false, true, pKiller);
                break;
            case 3:
                NachrichtSenden(WITK_TEXTID_KILLER_INFO3, false, true, pKiller);
                pKiller->PlayDirectSound(SOUND_SIGNAL, pKiller);
                break;
            case 4:
                NachrichtSenden(WITK_TEXTID_KILLER_INFO4, false, true, pKiller);
                break;
            case 5:
                NachrichtSenden(WITK_TEXTID_KILLER_INFO5, false, true, pKiller);
                break;
            case 6:
                // Leiste Verbleibende Leben wird unsichtbar gemacht
                AktualisierungsPacketSenden(UI_LEISTE_LEBEN, 0, pKiller);
                // Modifikationen werden entfernt
                KillerModifikationenEntfernen(pKiller);
                // Damit der Killer beim Abstimmen wie ein Camper aussieht
                CamperModifikationenSetzen(pKiller);
                // Position von Mr. Who
                // ToDo: vll. andere IconID
                SpielerSignalSenden(pKiller, -10906.996094f, -364.914276f, sWhoIsTheKillerMgr.GetEinstellung(KILLER_SIG_ICON), 7, 6, false, "Mr. Who");
                // Nachricht...
                NachrichtSenden(WITK_TEXTID_KILLER_INFO_ABSTIMMEN, false, true, pKiller);
                // Der Killer wird zu Mr. Who teleportiert
                SpielerTeleportieren(pKiller, WITK_TELEPORT_WEGKREUZUNG);
                break;
            case 7:
                if (KillerIdentifiziert())
                {
                    // Der Killer bekommt sein normales Aussehen
                    pKiller->DeMorph();
                    // Feindliche Fraktion
                    pKiller->setFaction(FRAKTION_FEINDLICH);
                    // Position vom Teleportfelsen
                    // ToDo: vll. andere IconID
                    SpielerSignalSenden(pKiller, -10709.489258f, -420.599213f, sWhoIsTheKillerMgr.GetEinstellung(KILLER_SIG_ICON), 7, 6, false, "Teleportfelsen");
                    // Nachricht... Jemand hat den Killer erkannt
                    NachrichtSenden(WITK_TEXTID_KILLER_ERKANNT_KILLER, false, true, pKiller);
                    // Der Killer wird in seine Starthuette teleportiert
                    SpielerTeleportieren(pKiller, WITK_TELEPORT_KILLER_START);
                }
                else
                {
                    CamperModifikationenEntfernen(pKiller);
                    // Nachricht... Kein Spieler konnte Killer finden
                    NachrichtSenden(WITK_TEXTID_NICHT_ERKANNT_KILLER, false, true, pKiller);
                    // Der Killer bekommt eine Belohnung
                    SpielerBelohnung(pKiller, STATUS_GEWONNEN);
                    // "STATUS_SCHLUSS" beginnt
                    SetSpielStatus(STATUS_SCHLUSS);
                }
                break;
            case 9:
                SignalCamperSenden(pKiller, true);
                break;
        }
    }
}

// Verarbeitet die gesetzten Aktionen von einem Camper
void WhoIsTheKillerSpiel::CamperThreadsAbarbeiten(Player* pCamper)
{
    if (!pCamper)
        return;

    // Camper Aktion-Threads
    for (std::vector<uint8>::iterator itr = m_mCamper[pCamper->GetObjectGuid()].vAktionThreads.begin(); itr != m_mCamper[pCamper->GetObjectGuid()].vAktionThreads.end(); itr = m_mCamper[pCamper->GetObjectGuid()].vAktionThreads.erase(itr))
    {
        switch (*itr)
        {
            case 128:
                // Camper wird wiederbelebt falls er tot ist
                SpielerWiederbeleben(pCamper);
                // Camper steigt vom Mount
                if (pCamper->IsMounted())
                {
                    pCamper->Unmount();
                    pCamper->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);
                }
                // Camper wird zum Daemmerwald teleportiert
                SpielerTeleportieren(pCamper, urand(0, WITK_TELEPORT_ZUFALL_MAX));
                break;
            case 129:
                CamperModifikationenSetzen(pCamper);
                NachrichtSenden(WITK_TEXTID_CAMPER_INFO1, false, true, pCamper);
                break;
            case 130:
                NachrichtSenden(WITK_TEXTID_CAMPER_INFO2, false, true, pCamper);
                break;
            case 131:
                NachrichtSenden(WITK_TEXTID_CAMPER_INFO3, false, true, pCamper);
                break;
            case 132:
                // Leiste Verbleibende Leben wird unsichtbar gemacht
                AktualisierungsPacketSenden(UI_LEISTE_LEBEN, 0, pCamper);
                // Position von Mr. Who
                // ToDo: vll. andere IconID
                SpielerSignalSenden(pCamper, -10906.996094f, -364.914276f, sWhoIsTheKillerMgr.GetEinstellung(KILLER_SIG_ICON), 7, 6, false, "Mr. Who");
                // Nachricht...
                char cNachricht[100];
                sprintf(cNachricht, GetNachricht(WITK_TEXTID_CAMPER_INFO_ABSTIMMEN), 60);
                NachrichtSenden(cNachricht, false, true, pCamper);
                // Der Camper wird zu Mr. Who teleportiert
                SpielerTeleportieren(pCamper, WITK_TELEPORT_WEGKREUZUNG);
                break;
            case 133:
                // Richtig abgestimmt
                if (m_mCamper[pCamper->GetObjectGuid()].bAbstimmen)
                {
                    // Der Camper bekommt wieder sein normales Aussehen
                    pCamper->DeMorph();
                    // Position vom Teleportfelsen
                    // ToDo: vll. andere IconID
                    SpielerSignalSenden(pCamper, -10709.489258f, -420.599213f, sWhoIsTheKillerMgr.GetEinstellung(KILLER_SIG_ICON), 7, 6, false, "Teleportfelsen");
                    // Nachricht... Richtig abgestimmt
                    NachrichtSenden(WITK_TEXTID_CAMPER_RICHTIG_ABGESTIMMT, false, true, pCamper);
                    // Der Camper wird an einen zufaelligen Ort teleportiert
                    SpielerTeleportieren(pCamper, urand(0, WITK_TELEPORT_ZUFALL_MAX));
                }
                // Falsch abgestimmt
                else
                {
                    // Modifikationen werden entfernt
                    CamperModifikationenEntfernen(pCamper);
                    // Nachricht... Falsch abgestimmt
                    NachrichtSenden(WITK_TEXTID_CAMPER_FALSCH_ABGESTIMMT, false, true, pCamper);
                    // Der Camper wird abgemeldet
                    SpielVerlassen(pCamper->GetObjectGuid());
                    // Der Camper wird in seinen Spielertreff teleportiert
                    SpielerTeleportieren(pCamper, WITK_TELEPORT_SPIELERTREFF);
                    // Schleife / Funktion wird beendet
                    return;
                }
                break;
        }
    }
}

// Sendet die Position des Signalcampers an den Killer
void WhoIsTheKillerSpiel::SignalCamperSenden(Player* pKiller, bool bSignalSound)
{
    if (!pKiller)
        return;

    // Das Signal kann nicht gesendet werden...
    if (!pKiller->IsInWorld() || pKiller->IsBeingTeleported())
        return;

    if (Player* pSignalCamper = GetSpieler(m_mKiller[pKiller->GetObjectGuid()].SignalCamperGuid))
    {
        SpielerSignalSenden(pKiller, pSignalCamper->GetPositionX(), pSignalCamper->GetPositionY(), sWhoIsTheKillerMgr.GetEinstellung(KILLER_SIG_ICON), 7, 6, bSignalSound, pSignalCamper->GetName());
    }
    else
        SignalCamperWechseln(pKiller->GetObjectGuid());
}

// Wechselt den Signalcamper nach der Reihe
void WhoIsTheKillerSpiel::SignalCamperWechseln(ObjectGuid KillerGuid)
{
    // Die Campermap darf nicht leer sein...
    if (m_mCamper.size() == 0)
        return;

    CamperMap::iterator itr = m_mCamper.find(m_mKiller[KillerGuid].SignalCamperGuid);

    // Der Signalcamper ist gar nicht eingetragen...
    if (itr == m_mCamper.end())
    {
        itr = m_mCamper.begin();
    }
    else
    {
        ++itr;

        // Wenn er am Ende angekommen ist, oder nur 1 Element vorhanden
        if (itr == m_mCamper.end())
            itr = m_mCamper.begin();
    }

    m_mKiller[KillerGuid].SignalCamperGuid = itr->first;
}

// Die Modifikationen fuer den Killer werden gesetzt
void WhoIsTheKillerSpiel::KillerModifikationenSetzen(Player* pKiller)
{
    if (pKiller)
    {
        // Der Killer wird wiederbelebt, wenn er tot ist
        SpielerWiederbeleben(pKiller);
        // Phase setzen
        pKiller->SetPhaseMask(sWhoIsTheKillerMgr.GetEinstellung(PHASE), true);
        // Killer wird unsichtbar
        pKiller->SetVisibility(VISIBILITY_OFF);
        // Killer bekommt sein neues Aussehen
        pKiller->SetDisplayId(sWhoIsTheKillerMgr.GetEinstellung(KILLER_MODEL));
        // Freundliche Fraktion
        pKiller->setFaction(FRAKTION_NEUTRAL);
        // Killer Geschwindigkeit wird gesetzt
        pKiller->UpdateSpeed(MOVE_WALK, true, float(sWhoIsTheKillerMgr.GetEinstellung(KILLER_GESCHW_BODEN)));
        pKiller->UpdateSpeed(MOVE_RUN, true, float(sWhoIsTheKillerMgr.GetEinstellung(KILLER_GESCHW_BODEN)));
        // Killer Action-Figur
        // Flug Geschwindigkeit vom Killer um 20% erhoeht
        if (pKiller->HasItemCount(ITEM_KILLER_ACTION_FIGUR, 1))
        {
            pKiller->UpdateSpeed(MOVE_FLIGHT, true, float(sWhoIsTheKillerMgr.GetEinstellung(KILLER_GESCHW_FLUG)) + float(sWhoIsTheKillerMgr.GetEinstellung(KILLER_GESCHW_FLUG) / 100 * 20));
        }
        else
            pKiller->UpdateSpeed(MOVE_FLIGHT, true, float(sWhoIsTheKillerMgr.GetEinstellung(KILLER_GESCHW_FLUG)));
        // Schaltet den Flugmodus fuer den Killer an
        WorldPacket data(SMSG_MOVE_SET_CAN_FLY, 12);
        data << pKiller->GetPackGUID();
        data << uint32(0);   
        pKiller->SendMessageToSet(&data, true);
    }
}

// Modifikationen die am Spieler vorgenommen wurden, werden entfernt
void WhoIsTheKillerSpiel::KillerModifikationenEntfernen(Player* pKiller)
{
    if (pKiller)
    {
        // Der Killer wird wiederbelebt, wenn er tot ist
        SpielerWiederbeleben(pKiller);
        // Der Killer bekommt sein normales Aussehen wieder
        pKiller->DeMorph();
        // Phase wieder normalisieren
        pKiller->SetPhaseMask(PHASEMASK_NORMAL, true);
        // Der Killer wird sichtbar
        pKiller->SetVisibility(VISIBILITY_ON);
        // Normale Fraktion
        pKiller->setFactionForRace(pKiller->getRace());
        // Die Killer Geschw. wird auf 1.0 gesetzt
        pKiller->UpdateSpeed(MOVE_WALK, true, 1.0f);
        pKiller->UpdateSpeed(MOVE_RUN, true, 1.0f);
        pKiller->UpdateSpeed(MOVE_FLIGHT, true, 1.0f);
        // Schaltet den Flugmodus fuer den Killer aus
        WorldPacket data(SMSG_MOVE_UNSET_CAN_FLY, 12);
        data << pKiller->GetPackGUID();
        data << uint32(0);   
        pKiller->SendMessageToSet(&data, true);
    }
}

// Die Modifikationen fuer den Camper werden gesetzt
void WhoIsTheKillerSpiel::CamperModifikationenSetzen(Player* pCamper)
{
    if (pCamper)
    {
        // Der Camper wird wiederbelebt, wenn er tot ist
        SpielerWiederbeleben(pCamper);
        // Der Camper bekommt sein neues Aussehen
        pCamper->SetDisplayId(sWhoIsTheKillerMgr.GetEinstellung(CAMPER_MODEL));
        // Freundliche Fraktion
        pCamper->setFaction(FRAKTION_NEUTRAL);
        // Phase setzen
        pCamper->SetPhaseMask(sWhoIsTheKillerMgr.GetEinstellung(PHASE), true);
    }
}

// Modifikationen die am Spieler vorgenommen wurden, werden entfernt
void WhoIsTheKillerSpiel::CamperModifikationenEntfernen(Player* pCamper)
{
    if (pCamper)
    {
        // Der Camper wird wiederbelebt, wenn er tot ist
        SpielerWiederbeleben(pCamper);
        // Camper bekommt sein normales Aussehen wieder
        pCamper->DeMorph();
        // Normale Fraktion
        pCamper->setFactionForRace(pCamper->getRace());
        // Phase wieder normalisieren
        pCamper->SetPhaseMask(PHASEMASK_NORMAL, true);
    }
}

// Teleportiert den Spieler mit der Guid "Guid" in den Spielertreff seiner Fraktion
void WhoIsTheKillerSpiel::OfflineTeleport(ObjectGuid Guid)
{
    if (Guid)
    {
        Team Fraktion = sObjectMgr.GetPlayerTeamByGUID(Guid);

        if (Fraktion == ALLIANCE)
        {
            // Goldhain
            Player::SavePositionInDB(Guid, 0, -9477.53f, 64.97f, 56.17f, 3.06f, 12);
        }
        else if (Fraktion == HORDE)
        {
            // Klingenhuegel
            Player::SavePositionInDB(Guid, 1, 312.27f, -4730.97f, 9.79f, 3.5f, 14);
        }
    }
}

// Teleportiert den Spieler an den Platz mit der Id "uiId"
void WhoIsTheKillerSpiel::SpielerTeleportieren(Player* pPlayer, uint8 uiId)
{
    if (!pPlayer)
        return;
    
    switch (uiId)
    {
        case 0: // Camper Start 1
            pPlayer->TeleportTo(0, -10530.499029f, 296.860107f, 30.965837f, 6.267021f); break;
        case 1: // Camper Start 2
            pPlayer->TeleportTo(0, -10385.581055f, 45.789993f, 55.488609f, 2.237927f); break;
        case 2: // Camper Start 3
            pPlayer->TeleportTo(0, -11071.836914f, 228.983948f, 31.181396f, 6.173547f); break;
        case 3: // Camper Start 4
            pPlayer->TeleportTo(0, -11113.205078f, -735.729980f, 56.762585f, 0.072568f); break;
        case 4: // Camper Start 5
            pPlayer->TeleportTo(0, -10648.652344f, -839.066528f, 59.501156f, 5.957689f); break;
        case 5: // Camper Start 6
            pPlayer->TeleportTo(0, -10248.145508f, -975.926025f, 45.400177f, 3.066520f); break;
        case 6: // Camper Start 7
            pPlayer->TeleportTo(0, -10339.190430f, -1543.405396f, 122.724648f, 3.014679f); break;
        case 7: // Camper Start 8
            pPlayer->TeleportTo(0, -10555.478516f, -1186.503052f, 28.025976f, 2.108253f); break;
        case WITK_TELEPORT_WEGKREUZUNG: // Wegkreuzung
            pPlayer->TeleportTo(0, -10920.946289f, -362.615906f, 39.550236f, 6.136547f); break;
        case WITK_TELEPORT_KILLER_START: // Killer Start
            pPlayer->TeleportTo(0, -11009.733398f, -1359.522217f, 54.353233f, 1.113155f); break;
        case WITK_TELEPORT_SPIELERTREFF: // Spielertreffs
            if (pPlayer->GetTeam() == ALLIANCE)
            { // Goldhain
                pPlayer->TeleportTo(0, -9477.53f, 64.97f, 56.17f, 3.06f);
            }
            else // Klingenhuegel
                pPlayer->TeleportTo(1, 312.27f, -4730.97f, 9.79f, 3.5f);
            break;
    }

    // Debug
    sLog.outDebug(">> Spieler '%s' wurde zu Id '%u' teleportiert", pPlayer->GetName(), uiId);
}

// Der Spieler wird wiederbelebt falls er tot ist
void WhoIsTheKillerSpiel::SpielerWiederbeleben(Player* pPlayer)
{
    if (!pPlayer)
        return;

    // Der Spieler ist noch am Leben
    if (pPlayer->isAlive())
        return;

    pPlayer->ResurrectPlayer(0.5f);
    pPlayer->SpawnCorpseBones();
    pPlayer->SaveToDB();

    // Debug
    sLog.outDebug(">> Spieler '%s' wurde wiederbelebt", pPlayer->GetName());
}

// Sendet ein Signal an einen Spieler
void WhoIsTheKillerSpiel::SpielerSignalSenden(Player* pPlayer, float x, float y, uint32 uiIconId, uint32 uiFlags, uint32 uiData, bool bSignalSound, char const* cName)
{
    if (!pPlayer)
        return;

    // Signal Sound
    if (bSignalSound)
        pPlayer->PlayDirectSound(SOUND_SIGNAL, pPlayer);

    WorldPacket data(SMSG_GOSSIP_POI, (4+4+4+4+4+10));
    data << uiFlags;
    data << x << y;
    data << uint32(uiIconId);
    data << uint32(uiData);
    data << cName;
    pPlayer->GetSession()->SendPacket(&data);

    // Debug
    sLog.outDebug(">> Spieler '%s' wurde ein Signal gesendet", pPlayer->GetName());
}

// Der Spieler bekommt eine Belohnung
void WhoIsTheKillerSpiel::SpielerBelohnung(Player* pPlayer, WhoIsTheKillerSpielerStatus wSpielerStatus)
{
    uint32 uiItemEntry = ITEM_EVENTMUENZE;
    uint8 uiItemAnzahl = 0;

    if (wSpielerStatus == STATUS_GEWONNEN)
    {
        if (!urand(0, 10))
        {
            uiItemEntry = ITEM_GOLDMUENZE;
            uiItemAnzahl = sWhoIsTheKillerMgr.GetEinstellung(GM_GEWONNEN);
        }
        else
            uiItemAnzahl = sWhoIsTheKillerMgr.GetEinstellung(EV_GEWONNEN);
    }
    else if (wSpielerStatus == STATUS_UNENTSCHIEDEN)
    {
        uiItemAnzahl = sWhoIsTheKillerMgr.GetEinstellung(EV_UNENTSCHIEDEN);
    }
    else if (wSpielerStatus == STATUS_VERLOREN)
        uiItemAnzahl = sWhoIsTheKillerMgr.GetEinstellung(EV_VERLOREN);

    if (!uiItemAnzahl)
        return;

    // Gibt dem Spieler das neue Item mit Item Entry "uiItemEntry" und Anzahl "uiItemAnzahl"
    if (pPlayer->StoreNewItemInBestSlots(uiItemEntry, uiItemAnzahl))
    {
        if (uiItemEntry == ITEM_GOLDMUENZE)
        {
            if (uiItemAnzahl > 1)
            {
                char cNachricht[100];
                sprintf(cNachricht, GetNachricht(WITK_TEXTID_GM_BEKOMMEN_X), uiItemAnzahl);
                NachrichtSenden(cNachricht, true, false, pPlayer);
            }
            else
                NachrichtSenden(WITK_TEXTID_GM_BEKOMMEN_1, true, false, pPlayer);
        }
        else if (uiItemEntry == ITEM_EVENTMUENZE)
        {
            if (uiItemAnzahl > 1)
            {
                char cNachricht[100];
                sprintf(cNachricht, GetNachricht(WITK_TEXTID_EM_BEKOMMEN_X), uiItemAnzahl);
                NachrichtSenden(cNachricht, true, false, pPlayer);
            }
            else
                NachrichtSenden(WITK_TEXTID_EM_BEKOMMEN_1, true, false, pPlayer);
        }
    }
    else
        // Der Spieler hat keinen Platz mehr im Rucksack
        NachrichtSenden(WITK_TEXTID_KEIN_PLATZ_IM_RUCKSACK, true, false, pPlayer);
}

// Erstellt den Schluss-Countdown
bool WhoIsTheKillerSpiel::SchlussCountdown()
{
    char cNachricht[100];

    switch (m_uiSchlussTimer)
    {
        case 0:
            // Nachricht... Spiel endet jetzt
            NachrichtSenden(WITK_TEXTID_SPIEL_ENDET_JETZT, true, false, NULL);
            break;
        case 1:
            // Nachricht... Spiel endet in 1 Sekunde
            NachrichtSenden(WITK_TEXTID_SPIEL_ENDET_IN_1_SEK, true, false, NULL);
            break;
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 15:
        case 30:
        case 45:
            sprintf(cNachricht, GetNachricht(WITK_TEXTID_SPIEL_ENDET_IN_X_SEK), m_uiSchlussTimer);
            // Nachricht... Spiel endet in x Sekunden
            NachrichtSenden(cNachricht, true, false, NULL);
            break;
        case 60: // 1m
            // Nachricht... Spiel endet in 1 Minute
            NachrichtSenden(WITK_TEXTID_SPIEL_ENDET_IN_1_MIN, true, false, NULL);
            break;
        case 120: // 2m
        case 180: // 3m
        case 240: // 4m
        case 300: // 5m
        case 600: // 10m
            sprintf(cNachricht, GetNachricht(WITK_TEXTID_SPIEL_ENDET_IN_X_MIN), m_uiSchlussTimer / 60);
            // Nachricht... Spiel endet in x Minuten
            NachrichtSenden(cNachricht, true, false, NULL);
            break;
        default:
            break;
    }

    if (!m_uiSchlussTimer)
        return true;

    m_uiSchlussTimer--;

    return false;
}

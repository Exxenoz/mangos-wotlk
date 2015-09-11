#ifndef _WHO_IS_THE_KILLER_SPIEL_H
#define _WHO_IS_THE_KILLER_SPIEL_H

#include "SharedDefines.h"

class Player;
class WorldPacket;

enum WhoIsTheKillerNachrichten
{
    WITK_TEXTID_COUNTDOWN_X = -2006000,
    WITK_TEXTID_COUNTDOWN_ENDE = -2006001,
    WITK_TEXTID_KEIN_PLATZ_IM_RUCKSACK = -2006002,
    WITK_TEXTID_GM_BEKOMMEN_X = -2006003,
    WITK_TEXTID_EM_BEKOMMEN_X = -2006004,
    WITK_TEXTID_GM_BEKOMMEN_1 = -2006005,
    WITK_TEXTID_EM_BEKOMMEN_1 = -2006006,
    WITK_TEXTID_SPIELER_GEKILLT = -2006007,
    WITK_TEXTID_SPIELER_BESIEGT = -2006008,
    WITK_TEXTID_SPIELZONE_VERLASSEN = -2006009,
    WITK_TEXTID_KILLER_INFO1 = -2006010,
    WITK_TEXTID_KILLER_INFO2 = -2006011,
    WITK_TEXTID_KILLER_INFO_ABSTIMMEN = -2006012,
    WITK_TEXTID_KILLER_ERKANNT_KILLER = -2006013,
    WITK_TEXTID_KILLER_INFO3 = -2006014,
    WITK_TEXTID_KILLER_INFO4 = -2006015,
    WITK_TEXTID_KILLER_INFO5 = -2006016,
    WITK_TEXTID_CAMPER_INFO_ABSTIMMEN = -2006017,
    WITK_TEXTID_CAMPER_FALSCH_ABGESTIMMT = -2006018,
    WITK_TEXTID_CAMPER_RICHTIG_ABGESTIMMT = -2006019,
    WITK_TEXTID_CAMPER_INFO1 = -2006020,
    WITK_TEXTID_CAMPER_INFO2 = -2006021,
    WITK_TEXTID_CAMPER_INFO3 = -2006022,
    WITK_TEXTID_ALLE_CAMPER_BESIEGT = -2006023,
    WITK_TEXTID_KILLER_OFFLINE = -2006024,
    WITK_TEXTID_PORTAL_ENTKOMMEN_KILLER = -2006025,
    WITK_TEXTID_KILLER_BESIEGT_CAMPER = -2006026,
    WITK_TEXTID_ALLE_CAMPER_OFFLINE = -2006027,
    WITK_TEXTID_NICHT_ERKANNT_KILLER = -2006028,
    WITK_TEXTID_CAMPER_BESIEGT_CAMPER = -2006029,
    WITK_TEXTID_PORTAL_ENTKOMMEN_CAMPER = -2006030,
    WITK_TEXTID_KILLER_BESIEGT_KILLER = -2006031,
    WITK_TEXTID_UNENTSCHIEDEN = -2006032,
    WITK_TEXTID_KANN_NICHT_VERWENDET_WERDEN = -2006033,
    WITK_TEXTID_ZUERST_KILLER_ANKLICKEN = -2006034,
    WITK_TEXTID_ZUERST_CAMPER_ANKLICKEN = -2006035,
    WITK_TEXTID_KILLER_VOR_DIR_DURCH_PORTAL = -2006036,
    WITK_TEXTID_SPIELER_IST_KEIN_CAMPER = -2006037,
    WITK_TEXTID_KILLER_ZU_WEIT_ENTFERNT = -2006038,
    WITK_TEXTID_CAMPER_ZU_WEIT_ENTFERNT = -2006039,
    WITK_TEXTID_CAMPER_MUSS_ZUERST_GEFANGEN_WERDEN = -2006040,
    WITK_TEXTID_CAMPER_WURDE_BEREITS_GEFANGEN = -2006041,
    WITK_TEXTID_FAKE_CAMPER_BEREITS_BESIEGT = -2006042,
    WITK_TEXTID_BRAUCHT_KILLER_FIGUR = -2006043,
    WITK_TEXTID_COOLDOWN_KILLEN_X = -2006044,
    WITK_TEXTID_COOLDOWN_KILLEN_1 = -2006045,
    WITK_TEXTID_COOLDOWN_VORBEI = -2006046,
    WITK_TEXTID_BEREITS_ANDEREN_CAMPER_GEFANGEN = -2006047,
    WITK_TEXTID_SPIEL_ENDET_IN_X_SEK = -2006048,
    WITK_TEXTID_SPIEL_ENDET_IN_1_SEK = -2006049,
    WITK_TEXTID_SPIEL_ENDET_IN_X_MIN = -2006050,
    WITK_TEXTID_SPIEL_ENDET_IN_1_MIN = -2006051,
    WITK_TEXTID_SPIEL_ENDET_JETZT = -2006052,
    WITK_TEXTID_SPIELER_NORMAL = -2006053,
    WITK_TEXTID_SPIELER_KICKEN = -2006054
};

enum WhoIsTheKillerTeleport
{
    WITK_TELEPORT_ZUFALL_MAX = 7,
    WITK_TELEPORT_WEGKREUZUNG = 8,
    WITK_TELEPORT_KILLER_START = 9,
    WITK_TELEPORT_SPIELERTREFF = 10
};

enum WhoIsTheKillerSpielStatus
{
    STATUS_WARTEN        = 0,    // Erster Status, es wird gewartet bis sich genug Spieler angemeldet haben
    STATUS_COUNTDOWN    = 1,    // Zweiter Status, es sind genug Spieler angemeldet und der Countdown wurde gestartet
    STATUS_KILLEN        = 2,    // Dritter Status, der Killer ist am killen
    STATUS_ABSTIMMEN    = 3,    // Vierter Status, die Camper muessen abstimmen wer der Killer ist
    STATUS_FLUCHT        = 4,    // Fuenfter Status, der Killer ist auf der Flucht
    STATUS_SCHLUSS        = 5,    // Sechster Status, der Schlusstimer laeuft
    STATUS_ENDE            = 6        // Siebter Status, das Spiel ist zu Ende
};

enum WhoIsTheKillerWorldStates
{
    UI_LEISTE_ZEIT                    = 50000,
    UI_LEISTE_LEBEN                    = 50001,
    UI_ANZEIGE_VERBLEIBENDE_ZEIT_M    = 50010,
    UI_ANZEIGE_VERBLEIBENDE_ZEIT_S    = 50011,
    UI_ANZEIGE_VERBLEIBENDE_LEBEN    = 50012
};

enum WhoIsTheKillerSpielerStatus
{
    STATUS_GEWONNEN            = 0,
    STATUS_UNENTSCHIEDEN    = 1,
    STATUS_VERLOREN            = 2
};

enum WhoIsTheKillerFraktionen
{
    FRAKTION_NEUTRAL    = 35,
    FRAKTION_FEINDLICH    = 99
};

enum WhoIsTheKillerItems
{
    ITEM_EVENTMUENZE            = 800200,
    ITEM_GOLDMUENZE                = 800201,
    ITEM_KILLER_ACTION_FIGUR    = 800202,
    ITEM_CAMPER_ACTION_FIGUR    = 800203
};

enum WhoIsTheKillerSounds
{
    SOUND_ANMELDEN    = 880, // Sound der abgespielt wird, wenn sich ein Spieler anmeldet
    SOUND_ABMELDEN    = 882, // Sound der abgespielt wird, wenn sich ein Spieler abmeldet
    SOUND_SIGNAL    = 3175, // Sound der abgespielt wird, wenn ein Spieler ein Signal bekommt
    SOUND_WARNUNG    = 50001 // Sound der abgespielt wird, wenn der Killer sich einem Camper naehert
};

enum WhoIsTheKillerSpells
{
    SPELL_SCHLAF    = 20989,
    SPELL_NETZ        = 38661
};

// Die Killer Struktur
struct Killer
{
    public:
        Killer() { uiFakeLeben = 1; uiZonenTimer = 0; uiCooldownKillen = 0; bKillerFigur = true; SignalCamperGuid.Clear(); vAktionThreads.clear(); }
        Killer(uint8 _uiFakeLeben) { uiFakeLeben = _uiFakeLeben; uiZonenTimer = 0; uiCooldownKillen = 0; bKillerFigur = true; SignalCamperGuid.Clear(); vAktionThreads.clear(); }

        uint8 uiFakeLeben;
        uint32 uiZonenTimer;
        uint32 uiCooldownKillen;
        bool bKillerFigur;
        ObjectGuid SignalCamperGuid;
        std::vector<uint8> vAktionThreads;
};

// Die Camper Struktur
struct Camper
{
    public:
        Camper() { uiLeben = 1; uiWiederbelebungsTimer = 0; uiZonenTimer = 0; bAbstimmen = false; vAktionThreads.clear(); }
        Camper(uint8 _uiLeben) { uiLeben = _uiLeben; uiWiederbelebungsTimer = 0; uiZonenTimer = 0; bAbstimmen = false; vAktionThreads.clear(); }

        uint8 uiLeben;
        uint32 uiWiederbelebungsTimer;
        uint32 uiZonenTimer;
        bool bAbstimmen;
        std::vector<uint8> vAktionThreads;
};

typedef std::map<ObjectGuid, bool> SpielerMap;
typedef std::map<ObjectGuid, Killer> KillerMap;
typedef std::map<ObjectGuid, Camper> CamperMap;

class MANGOS_DLL_SPEC WhoIsTheKillerSpiel
{
    public:
        WhoIsTheKillerSpiel(ObjectGuid LeiterGuid, std::string sSpielName);
        ~WhoIsTheKillerSpiel();

        // Wird jede Sekunde (!) aufgerufen
        void Update();

        // ~~ Spielsystem ~~
        bool SpielStarten();
        void SpielBeitreten(ObjectGuid Guid);
        void SpielVerlassen(ObjectGuid Guid);
        void SpielBeenden();

        // ~~ Abstimmung ~~
        void Abstimmen(ObjectGuid CamperGuid, ObjectGuid TippGuid) { m_mCamper[CamperGuid].bAbstimmen = IstAlsKillerAngemeldet(TippGuid); }
        bool KillerIdentifiziert() { for (CamperMap::const_iterator itr = m_mCamper.begin(); itr != m_mCamper.end(); ++itr) { if (itr->second.bAbstimmen) { return true; } } return false; }

        // ~~ Anzeige ~~
        void UserInterface(ObjectGuid Guid, WorldPacket &data, uint32 &count);
        void AktualisierungsPacketSenden(uint32 uiFeld, uint32 uiWert, Player* pPlayer);
        void VerbleibendeZeitAktualisieren(Player* pPlayer);

        // ~~ Hooks ~~
        void AFK(Player* pPlayer);
        void ZonenWechsel(Player* pPlayer);
        void AreaWechsel(Player* pPlayer, uint32 uiAreaId);
        void KillerTot(Player* pKiller);
        void CamperTot(Player* pCamper);
        void KillerPortal(Player* pKiller);

        // ~~ Befehle ~~
        bool FigurWechseln(Player* pKiller);
        bool CamperKillen(Player* pKiller, Player* pCamper);
        bool CamperFangen(Player* pKiller, Player* pCamper);
        bool CamperVerwirren(Player* pKiller);
        bool SignalWechseln(Player* pKiller);

        // ~~ Nachrichtensystem ~~
        void NachrichtSenden(int32 iTextId, bool bChat, bool bScreen, Player* pPlayer, bool bKiller = true);
        void NachrichtSenden(const char* cNachricht, bool bChat, bool bScreen, Player* pPlayer, bool bKiller = true);

        // Set-Methoden
        void SetStatusKillenEnde(uint32 uiStatusKillenEnde) { m_uiStatusKillenEnde = uiStatusKillenEnde; }
        void SetStatusFluchtEnde(uint32 uiStatusFluchtEnde) { m_uiStatusFluchtEnde = uiStatusFluchtEnde; }
        void SetKillerAnzahl(uint8 uiKillerAnzahl) { m_uiKillerAnzahl = uiKillerAnzahl; }
        void SetLebenAnzahl(uint8 uiLebenAnzahl) { m_uiLebenAnzahl = uiLebenAnzahl; }

        // Get-Methoden
        ObjectGuid GetLeiterGuid() { return m_LeiterGuid; }
        std::string GetSpielName() { return m_sSpielName; }
        uint8 GetSpielerAnzahl() { return m_mSpielerGuids.size(); }

        WhoIsTheKillerSpielStatus GetSpielStatus() { return m_Status; }
        uint32 GetSpielZeit() { return m_uiSpielZeit; }

        uint32 GetStatusKillenEnde() { return m_uiStatusKillenEnde; }
        uint32 GetStatusFluchtEnde() { return m_uiStatusFluchtEnde; }
        uint8 GetKillerAnzahl() { return m_uiKillerAnzahl; }
        uint8 GetLebenAnzahl() { return m_uiLebenAnzahl; }

        Player* GetSpieler(ObjectGuid Guid);
        const char* GetNachricht(int32 iTextId);

        // Sonstige Get-Methoden
        bool IstAlsSpielerAngemeldet(ObjectGuid Guid) { return m_mSpielerGuids.find(Guid) != m_mSpielerGuids.end(); }
        bool IstAlsKillerAngemeldet(ObjectGuid Guid) { return m_mKiller.find(Guid) != m_mKiller.end(); }
        bool IstAlsCamperAngemeldet(ObjectGuid Guid) { return m_mCamper.find(Guid) != m_mCamper.end(); }
        bool IstAlsKillerOderCamperAngemeldet(ObjectGuid Guid) { return IstAlsKillerAngemeldet(Guid) || IstAlsCamperAngemeldet(Guid); }

        // ScriptDev2 Hacks
        void ErstelleSpielerListe(Player* pPlayer, uint8 uiIconId, uint32 uiSender);
        void ErstelleAbstimmungsListe(Player* pPlayer, uint8 uiIconId, uint32 uiSender);

    private:
        ObjectGuid m_LeiterGuid;
        std::string m_sSpielName;
        SpielerMap m_mSpielerGuids;

        // Figuren
        KillerMap m_mKiller;
        CamperMap m_mCamper;

        // Timer
        uint32 m_uiCountdown;
        uint32 m_uiSpielZeit;
        uint32 m_uiSignalTimer;
        uint32 m_uiSchlussTimer;

        // Optionen
        uint32 m_uiStatusKillenEnde;
        uint32 m_uiStatusFluchtEnde;
        uint8 m_uiKillerAnzahl;
        uint8 m_uiLebenAnzahl;

        // Spielstatus
        WhoIsTheKillerSpielStatus m_Status;

        // ~~ Spielsystem ~~
        bool SpielFigurenZuteilen();

        // ~~ Spielgeschehen ~~
        void SpielGeschehen();
        void SpielGeschehenKiller(ObjectGuid Guid);
        void SpielGeschehenCamper(ObjectGuid Guid);
        void KillerThreadsAbarbeiten(Player* pKiller);
        void CamperThreadsAbarbeiten(Player* pCamper);

        // ~~ Signalcamper ~~
        void SignalCamperSenden(Player* pKiller, bool bSignalSound);
        void SignalCamperWechseln(ObjectGuid KillerGuid);

        // ~~ Modifikationen ~~
        void KillerModifikationenSetzen(Player* pKiller);
        void KillerModifikationenEntfernen(Player* pKiller);
        void CamperModifikationenSetzen(Player* pCamper);
        void CamperModifikationenEntfernen(Player* pCamper);

        // ~~ Teleportation ~~
        void OfflineTeleport(ObjectGuid Guid);
        void SpielerTeleportieren(Player* pPlayer, uint8 uiId);

        // ~~ Sonstiges ~~
        void SpielerWiederbeleben(Player* pPlayer);
        void SpielerSignalSenden(Player* pPlayer, float x, float y, uint32 uiIconId, uint32 uiFlags, uint32 uiData, bool bSignalSound, char const* cName);
        void SpielerBelohnung(Player* pPlayer, WhoIsTheKillerSpielerStatus wSpielerStatus);
        bool SchlussCountdown();

        // Set-Methoden
        void SetSpielStatus(WhoIsTheKillerSpielStatus Status)
        {
            // ToDo: Evtl. woanders hin verschieben
            if (Status == STATUS_SCHLUSS)
            {
                // Leiste Verbleibende Zeit wird unsichtbar gemacht
                AktualisierungsPacketSenden(UI_LEISTE_ZEIT, 0, NULL);

                // Falls STATUS_SCHLUSS in STATUS_KILLEN startet
                if (m_Status == STATUS_KILLEN)
                {
                    // Leiste Verbleibende Leben wird unsichtbar gemacht
                    AktualisierungsPacketSenden(UI_LEISTE_LEBEN, 0, NULL);
                }
            }

            m_Status = Status;
        }
};
#endif

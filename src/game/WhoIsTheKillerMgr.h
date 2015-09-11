#ifndef _WHO_IS_THE_KILLER_MGR_H
#define _WHO_IS_THE_KILLER_MGR_H

#include "SharedDefines.h"
#include "Policies/Singleton.h"
#include "WhoIsTheKillerSpiel.h"

class Player;
class WorldPacket;

enum WhoIsTheKillerEinstellungen
{
    COUNTDOWN = 0, // Countdown in Sekunden bis zum eigentlichen Spielstart
    PHASE, // Phase in der gespielt wird
    CAMPER_MIN, // Wieviele Camper man mindestens braucht um das Spiel als Leiter zu starten
    CAMPER_MODEL, // Arbeiter
    CAMPER_WBLT, // Sekunden die der Camper warten muss bis er wiederbelebt wird
    KILLER_MODEL, // Gargoyle
    KILLER_GESCHW_BODEN, // Boden-Geschwindigkeit des Killers
    KILLER_GESCHW_FLUG, // Flug-Geschwindigkeit des Killers
    KILLER_CD_KILLEN, // Sekunden die der Killer warten muss, bis das gefangene Opfer gekillt werden kann
    KILLER_SIG_ICON, // ID des Killer Signal Icon
    KILLER_SIG_IV, // Sekunden Interval in dem der Killer Signale bekommt
    GM_GEWONNEN, // Anzahl der Goldmuenzen, die der Spieler bekommt, wenn er gewonnen hat
    EV_GEWONNEN, // Anzahl der Eventmuenzen, die der Spieler bekommt, wenn er gewonnen hat
    EV_UNENTSCHIEDEN, // Anzahl der Eventmuenzen, die der Spieler bekommt, wenn das Spiel unentschieden endet
    EV_VERLOREN, // Anzahl der Eventmuenzen, die der Spieler bekommt, wenn er verloren hat
    ZONEN_T_WECHSEL, // Bei Zonenwechsel wird der Spieler nach Ablauf dieser Zeit in Sekunden teleportiert
    AKTIONS_ENTFERNUNG_MAX, // Maximale Entfernung zw. Killer und Camper, in diesem Bereich koennen Aktionen durchgefuehrt werden
    MAX_ERSTELLTE_SPIELE, // Maximale Anzahl an erstellten Spielen
    MAX_SPIELER_ANZAHL, // Maximale Anzahl von Spielern pro Spiel
    MAX_ANZAHL_LEBEN, // Maximale Anzahl von Leben, die man auswaehlen kann
    MAX_ANZAHL_KILLER, // Maximale Anzahl von Killern, die man auswaehlen kann
    MAX_ZEICHEN_SPIEL_NAME, // Maximale Anzahl von Zeichen, die der Spielname lang sein darf
    EINSTELLUNGS_ANZAHL // Anzahl der Einstellungen
};

typedef std::map<uint8, WhoIsTheKillerSpiel*> SpieleMap;

class WhoIsTheKillerMgr
{
    public:
        WhoIsTheKillerMgr();
        ~WhoIsTheKillerMgr();

        void EinstellungenLaden();

        void Update(uint32 uiDiff);

        // ~~ Spiel System ~~
        WhoIsTheKillerSpiel* SpielErstellen(ObjectGuid LeiterGuid, const char* cSpielName);
        void SpielEntfernen(WhoIsTheKillerSpiel* pSpiel);

        // ~~ Get / Set Methoden ~~
        WhoIsTheKillerSpiel* GetSpiel(ObjectGuid Guid);
        WhoIsTheKillerSpiel* GetSpiel(uint8 uiSpielId);
        uint8 GetSpieleAnzahl() { return m_mSpiele.size(); }

        int32 GetEinstellung(uint8 uiIdx) { return m_iEinstellungen[uiIdx]; }

        // ~~ ScriptDev2 Hacks ~~
        void ErstelleSpieleListe(Player* pPlayer, uint8 uiIconId, uint32 uiSender);

    private:
        // ~~ Klassenattribute ~~
        uint32 m_uiMinutenTimer;
        uint32 m_uiSekundenTimer;

        SpieleMap m_mSpiele;
        uint8 m_uiSpieleIndexZaehler;

        int32 m_iEinstellungen[EINSTELLUNGS_ANZAHL];

        // ~~ Get Methoden ~~
        Player* GetSpieler(ObjectGuid Guid);
};

#define sWhoIsTheKillerMgr MaNGOS::Singleton<WhoIsTheKillerMgr>::Instance()
// ** ScriptDev2 ** Who is the Killer Funktionen **
class MANGOS_DLL_SPEC sWhoIsTheKillerMgrSD2
{
    public:
        static WhoIsTheKillerSpiel* GetSpiel(ObjectGuid Guid);
        static WhoIsTheKillerSpiel* GetSpiel(uint8 uiSpielId);
        static uint8 GetSpieleAnzahl();

        static WhoIsTheKillerSpiel* SpielErstellen(ObjectGuid LeiterGuid, const char* cSpielName);
        static void SpielEntfernen(WhoIsTheKillerSpiel* pSpiel);

        static void ErstelleSpieleListe(Player* pPlayer, uint8 uiIconId, uint32 uiSender);

        static int32 GetEinstellung(uint8 uiIdx);
};
#endif

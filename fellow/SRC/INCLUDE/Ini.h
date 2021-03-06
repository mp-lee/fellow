#ifndef INI_H
#define INI_H


/*============================================================================*/
/* struct that holds initialization data                                      */
/*============================================================================*/

typedef struct {

  STR	m_description[256];
	
  /*==========================================================================*/
  /* Holds current used configuration filename                                */
  /*==========================================================================*/

  STR  m_current_configuration[CFG_FILENAME_LENGTH];

  /*==========================================================================*/
  /* Window positions (Main Window, Emulation Window)                         */
  /*==========================================================================*/

  ULO  m_mainwindowxposition;
  ULO  m_mainwindowyposition;
  ULO  m_emulationwindowxposition;
  ULO  m_emulationwindowyposition;

  /*==========================================================================*/
  /* History of used config files                                             */
  /*==========================================================================*/

  STR  m_configuration_history[4][CFG_FILENAME_LENGTH];
  
  /*==========================================================================*/
  /* Holds last used directories                                              */
  /*==========================================================================*/

  STR  m_lastusedkeydir[CFG_FILENAME_LENGTH];
  STR  m_lastusedkickimagedir[CFG_FILENAME_LENGTH];
  STR  m_lastusedconfigurationdir[CFG_FILENAME_LENGTH];
  ULO  m_lastusedconfigurationtab;
  STR  m_lastusedglobaldiskdir[CFG_FILENAME_LENGTH];
  STR  m_lastusedhdfdir[CFG_FILENAME_LENGTH];
  STR  m_lastusedmoddir[CFG_FILENAME_LENGTH];
  STR  m_lastusedstatefiledir[CFG_FILENAME_LENGTH];
  STR  m_lastusedpresetromdir[CFG_FILENAME_LENGTH];
  
} ini;

extern ini* wgui_ini;

/*============================================================================*/
/* struct ini property access functions                                       */
/*============================================================================*/

extern ULO iniGetMainWindowXPos(ini *);
extern ULO iniGetMainWindowYPos(ini *);
extern ULO iniGetEmulationWindowXPos(ini *);
extern ULO iniGetEmulationWindowYPos(ini *);
 
extern void iniSetMainWindowPosition(ini *initdata, ULO mainwindowxpos, ULO mainwindowypos);
extern void iniSetEmulationWindowPosition(ini *initdata, ULO emulationwindowxpos, ULO emulationwindowypos);
extern STR *iniGetConfigurationHistoryFilename(ini *initdata, ULO position);
extern void iniSetConfigurationHistoryFilename(ini *initdata, ULO position, STR *configuration);
extern STR *iniGetConfigurationHistoryFilename(ini *initdata, ULO position);
extern void iniSetConfigurationHistoryFilename(ini *initdata, ULO position, STR *cfgfilename);
extern STR *iniGetCurrentConfigurationFilename(ini *initdata);
extern void iniSetCurrentConfigurationFilename(ini *initdata, STR *configuration);
extern void iniSetLastUsedCfgDir(ini *initdata, STR *directory);
extern STR *iniGetLastUsedCfgDir(ini *initdata);
extern void iniSetLastUsedKickImageDir(ini *initdata, STR *directory);
extern STR *iniGetLastUsedKickImageDir(ini *initdata);
extern void iniSetLastUsedKeyDir(ini *initdata, STR *directory);
extern STR *iniGetLastUsedKeyDir(ini *initdata);
extern void iniSetLastUsedGlobalDiskDir(ini *initdata, STR *directory);
extern STR *iniGetLastUsedGlobalDiskDir(ini *initdata);
extern void iniSetLastUsedHdfDir(ini *initdata, STR *directory);
extern STR *iniGetLastUsedHdfDir(ini *initdata);
extern void iniSetLastUsedModDir(ini *initdata, STR *directory);
extern STR *iniGetLastUsedModDir(ini *initdata);
extern void iniSetLastUsedCfgTab(ini *initdata, ULO cfgTab);
extern ULO iniGetLastUsedCfgTab(ini *initdata);
extern void iniSetLastUsedStateFileDir(ini *initdata, STR *directory);
extern STR *iniGetLastUsedStateFileDir(ini *initdata);
extern void iniSetLastUsedPresetROMDir(ini *initdata, STR *directory);
extern STR *iniGetLastUsedPresetROMDir(ini *initdata);

extern BOOLE iniSetOption(ini *initdata, STR *initoptionstr);
extern BOOLE iniSaveOptions(ini *initdata, FILE *inifile);

/*============================================================================*/
/* struct iniManager                                                          */
/*============================================================================*/

typedef struct {
  ini *m_current_ini;
  ini *m_default_ini;
} iniManager;

/*============================================================================*/
/* struct iniManager property access functions                                */
/*============================================================================*/

extern void iniManagerSetCurrentInitdata(iniManager *initdatamanager, ini *currentinitdata);
extern ini *iniManagerGetCurrentInitdata(iniManager *initdatamanager);
extern void iniManagerSetDefaultInitdata(iniManager *inimanager, ini *initdata);
extern ini *iniManagerGetDefaultInitdata(iniManager *inimanager);


/*============================================================================*/
/* struct iniManager utility functions                                        */
/*============================================================================*/

extern BOOLE iniManagerInitdataActivate(iniManager *initdatamanager);
extern ini *iniManagerGetNewInitdata(iniManager *initdatamanager);
extern void iniManagerFreeInitdata(iniManager *initdatamanager, ini *initdata);
extern void iniManagerStartup(iniManager *initdatamanager);
extern void iniManagerShutdown(iniManager *initdatamanager);


/*============================================================================*/
/* The actual iniManager instance                                             */
/*============================================================================*/

extern iniManager ini_manager;

extern void iniEmulationStart(void);
extern void iniEmulationStop(void);
extern void iniStartup(void);
extern void iniShutdown(void);


#endif  /* INI_H */

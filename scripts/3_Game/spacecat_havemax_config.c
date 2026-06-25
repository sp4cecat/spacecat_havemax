// spacecat_havemax - configuration layer
//
// Reads a JSON config from the server profile directory:
//     %profiles%/spacecat/havemax.json   ( = "$profile:spacecat/havemax.json" )
//
// The file is a JSON OBJECT with two global flags and an "items" array:
//
// {
//     "show_notifications": true,
//     "log_events": true,
//     "items": [
//         { "className": "NVGoggles", "max": 1, "slots": ["NVG", "Eyewear"],
//           "notification": "You may only carry one pair of night-vision goggles." },
//         { "className": "Canteen", "max": 2, "slots": [],
//           "notification": "You can carry at most two canteens." }
//     ]
// }
//
//   show_notifications : when true, a blocked player is shown the matching
//                        entry's "notification" text (if non-blank).
//   log_events         : when true, every blocked attempt is logged to the
//                        server script log / RPT.
//
//   Per item (in "items"):
//   className    : item config class to limit (case-insensitive; base classes
//                  match subclasses via the config inheritance chain).
//   max          : maximum number allowed anywhere in a single player's
//                  inventory hierarchy (hands + clothing + every container).
//   slots        : OPTIONAL list of attachment slot names the item may occupy.
//                  - empty / omitted -> no slot restriction (count rule only)
//                  - non-empty       -> the item may ONLY be worn in one of these
//                                       slots: any other slot is denied and loose
//                                       cargo placement is denied. (It can still
//                                       pass through hands so it can be equipped.)
//   notification : OPTIONAL message shown to the player when this item is blocked
//                  (only if show_notifications is true). Blank = no message.

class SpacecatHaveMaxEntry
{
    string              className;
    int                 max;
    ref array<string>   slots;
    string              notification;

    void SpacecatHaveMaxEntry()
    {
        max          = 0;
        slots        = new array<string>();
        notification = "";
    }

    // True when this entry pins the item to a fixed set of attachment slots.
    bool HasSlotRestriction()
    {
        return slots && slots.Count() > 0;
    }

    // Case-insensitive slot-name membership test.
    bool AllowsSlotName(string slotName)
    {
        if (!HasSlotRestriction())
            return true;

        string wanted = slotName;
        wanted.ToLower();

        for (int i = 0; i < slots.Count(); i++)
        {
            string s = slots[i];
            s.ToLower();
            if (s == wanted)
                return true;
        }
        return false;
    }
}

// Root object deserialized from havemax.json.
class SpacecatHaveMaxData
{
    bool                                show_notifications;
    bool                                log_events;
    ref array<ref SpacecatHaveMaxEntry> items;

    void SpacecatHaveMaxData()
    {
        show_notifications = false;
        log_events         = false;
        items              = new array<ref SpacecatHaveMaxEntry>();
    }
}

class SpacecatHaveMaxConfig
{
    static const string DIR_PATH      = "$profile:spacecat";
    static const string FILE_PATH     = "$profile:spacecat/havemax.json";
    static const string EXAMPLE_PATH  = "$profile:spacecat/havemax.example.json";

    private static ref SpacecatHaveMaxConfig m_Instance;

    // Loaded rules, keyed by lower-cased className for O(1) lookup.
    private ref map<string, ref SpacecatHaveMaxEntry> m_Rules;
    // Resolution cache: lower-cased item type -> matched rule (or unset == miss).
    private ref map<string, ref SpacecatHaveMaxEntry> m_ResolveCache;
    private ref map<string, bool> m_ResolveMiss;
    private bool m_ShowNotifications;
    private bool m_LogEvents;
    private bool m_Loaded;

    void SpacecatHaveMaxConfig()
    {
        m_Rules             = new map<string, ref SpacecatHaveMaxEntry>();
        m_ResolveCache      = new map<string, ref SpacecatHaveMaxEntry>();
        m_ResolveMiss       = new map<string, bool>();
        m_ShowNotifications = false;
        m_LogEvents         = false;
        m_Loaded            = false;
    }

    static SpacecatHaveMaxConfig GetInstance()
    {
        if (!m_Instance)
        {
            m_Instance = new SpacecatHaveMaxConfig();
            m_Instance.Load();
        }
        return m_Instance;
    }

    bool IsEnabled()
    {
        return m_Rules && m_Rules.Count() > 0;
    }

    bool ShowNotifications()
    {
        return m_ShowNotifications;
    }

    bool LogEvents()
    {
        return m_LogEvents;
    }

    // Lookup honouring config-class inheritance: an entry for a base class
    // (e.g. "Plate_Carrier_Base") matches every item derived from it.
    SpacecatHaveMaxEntry GetEntryForType(string className)
    {
        if (!IsEnabled() || className == string.Empty)
            return null;

        string key = className;
        key.ToLower();

        // Cache fast-paths (populated by previous resolutions this session).
        SpacecatHaveMaxEntry cached;
        if (m_ResolveCache.Find(key, cached))
            return cached;
        if (m_ResolveMiss.Contains(key))
            return null;

        SpacecatHaveMaxEntry direct;
        if (m_Rules.Find(key, direct))
        {
            m_ResolveCache.Set(key, direct);
            return direct;
        }

        // Walk the CfgVehicles / CfgWeapons / CfgMagazines inheritance chain.
        string parent = GetConfigBaseName(className);
        int guard = 0;
        while (parent != string.Empty && guard < 64)
        {
            string pkey = parent;
            pkey.ToLower();

            SpacecatHaveMaxEntry inherited;
            if (m_Rules.Find(pkey, inherited))
            {
                m_ResolveCache.Set(key, inherited);
                return inherited;
            }

            parent = GetConfigBaseName(parent);
            guard++;
        }

        m_ResolveMiss.Set(key, true);
        return null;
    }

    // Resolve the immediate config parent of an item class across the three
    // CfgX roots that hold spawnable items.
    private string GetConfigBaseName(string className)
    {
        array<string> roots = {"CfgVehicles", "CfgWeapons", "CfgMagazines"};
        for (int i = 0; i < roots.Count(); i++)
        {
            string path = roots[i] + " " + className;
            if (GetGame().ConfigIsExisting(path))
            {
                string baseName;
                GetGame().ConfigGetBaseName(path, baseName);
                return baseName;
            }
        }
        return string.Empty;
    }

    void Reload()
    {
        Load();
    }

    private void Load()
    {
        m_Rules.Clear();
        m_ResolveCache.Clear();
        m_ResolveMiss.Clear();
        m_ShowNotifications = false;
        m_LogEvents         = false;
        m_Loaded            = true;

        // Config lives in the SERVER profile - only the server authority cares.
        MakeDirectory(DIR_PATH);

        if (!FileExist(FILE_PATH))
        {
            WriteDefault();
            WriteExample();
            Log("no config found, wrote inert default at " + FILE_PATH + " (mod idle until edited)");
            return;
        }

        // Always refresh the human-readable example alongside the live file.
        WriteExample();

        SpacecatHaveMaxData data = new SpacecatHaveMaxData();
        string err;
        if (!JsonFileLoader<SpacecatHaveMaxData>.LoadFile(FILE_PATH, data, err))
        {
            LogError("failed to parse " + FILE_PATH + ": " + err + " (mod idle). Note: the config is now a JSON OBJECT with show_notifications/log_events/items, not a bare array.");
            return;
        }

        m_ShowNotifications = data.show_notifications;
        m_LogEvents         = data.log_events;

        int valid = 0;
        if (data.items)
        {
            for (int i = 0; i < data.items.Count(); i++)
            {
                SpacecatHaveMaxEntry e = data.items[i];
                if (!e || e.className == string.Empty)
                {
                    LogError("skipping config item " + i + ": missing className");
                    continue;
                }
                if (e.max < 0)
                {
                    LogError("skipping '" + e.className + "': max must be >= 0");
                    continue;
                }
                if (!e.slots)
                    e.slots = new array<string>();

                string key = e.className;
                key.ToLower();
                m_Rules.Set(key, e);
                valid++;
            }
        }

        Log("loaded " + valid + " rule(s) from " + FILE_PATH + " (notifications=" + m_ShowNotifications + ", logging=" + m_LogEvents + ")");
    }

    private void WriteDefault()
    {
        // Inert by default so installing the mod changes nothing until an
        // admin opts in by editing the file.
        SpacecatHaveMaxData empty = new SpacecatHaveMaxData();
        string err;
        if (!JsonFileLoader<SpacecatHaveMaxData>.SaveFile(FILE_PATH, empty, err))
            LogError("could not write default config: " + err);
    }

    private void WriteExample()
    {
        SpacecatHaveMaxData ex = new SpacecatHaveMaxData();
        ex.show_notifications = true;
        ex.log_events = true;

        SpacecatHaveMaxEntry nvg = new SpacecatHaveMaxEntry();
        nvg.className = "NVGoggles";
        nvg.max = 1;
        nvg.slots.Insert("NVG");
        nvg.slots.Insert("Eyewear");
        nvg.notification = "You may only carry one pair of night-vision goggles.";
        ex.items.Insert(nvg);

        SpacecatHaveMaxEntry plate = new SpacecatHaveMaxEntry();
        plate.className = "Plate_Carrier_Base";
        plate.max = 1;
        plate.notification = "You may only carry one plate carrier.";
        ex.items.Insert(plate);

        string err;
        JsonFileLoader<SpacecatHaveMaxData>.SaveFile(EXAMPLE_PATH, ex, err);
    }

    static void Log(string msg)
    {
        Print("[spacecat_havemax] " + msg);
    }

    static void LogError(string msg)
    {
        Error("[spacecat_havemax] " + msg);
    }
}

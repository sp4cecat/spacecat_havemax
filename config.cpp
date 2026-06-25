class CfgPatches
{
    class spacecat_havemax
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] = {"DZ_Data"};
    };
};

class CfgMods
{
    class spacecat_havemax
    {
        dir = "spacecat_havemax";
        picture = "";
        action = "";
        hideName = 1;
        hidePicture = 1;
        name = "spacecat_havemax";
        credits = "SpaceCat";
        author = "SpaceCat";
        authorID = "0";
        version = "1.0";
        extra = 0;
        type = "mod";

        dependencies[] = {"Game", "World", "Mission"};

        class defs
        {
            class gameScriptModule
            {
                value = "";
                files[] = {"spacecat_havemax/scripts/3_Game"};
            };
            class worldScriptModule
            {
                value = "";
                files[] = {"spacecat_havemax/scripts/4_World"};
            };
            class missionScriptModule
            {
                value = "";
                files[] = {"spacecat_havemax/scripts/5_Mission"};
            };
        };
    };
};

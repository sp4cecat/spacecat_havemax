// spacecat_havemax - server boot hook
//
// Loads (or reloads) the JSON config when the server mission initialises, so the
// rules are ready before any player connects. GetInstance() lazy-loads on first
// access too; the explicit Reload here guarantees a fresh read on every mission
// start (e.g. after a server restart within the same process).

modded class MissionServer
{
    override void OnInit()
    {
        super.OnInit();
        SpacecatHaveMaxConfig.GetInstance().Reload();
    }
}

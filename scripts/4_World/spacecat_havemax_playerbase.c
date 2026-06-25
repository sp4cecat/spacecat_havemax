// spacecat_havemax - player hooks
//
// Covers items landing DIRECTLY on the player: into hands, into the player's own
// cargo (e.g. a uniform's built-in slots resolved to the player), and onto the
// player's attachment slots (clothing, NVG, back, shoulder/melee, etc.). The
// hands hook is what catches "putting a container into your hands" - the whole
// nested contents of the picked-up container are counted by the shared logic.

modded class PlayerBase
{
    // Server-side timestamp (ms) of the last havemax denial reaction for this
    // player. Used by SpacecatHaveMaxLogic.Deny to throttle notifications/logs.
    protected int m_HaveMaxLastReact;

    int GetHaveMaxLastReact()
    {
        return m_HaveMaxLastReact;
    }

    void SetHaveMaxLastReact(int timeMs)
    {
        m_HaveMaxLastReact = timeMs;
    }

    override bool CanReceiveItemIntoCargo(EntityAI item)
    {
        if (!super.CanReceiveItemIntoCargo(item))
            return false;

        return SpacecatHaveMaxLogic.AllowReceive(this, item, -1, true, false);
    }

    override bool CanReceiveAttachment(EntityAI attachment, int slotId)
    {
        if (!super.CanReceiveAttachment(attachment, slotId))
            return false;

        return SpacecatHaveMaxLogic.AllowReceive(this, attachment, slotId, false, false);
    }

    override bool CanReceiveItemIntoHands(EntityAI item_to_hands)
    {
        if (!super.CanReceiveItemIntoHands(item_to_hands))
            return false;

        return SpacecatHaveMaxLogic.AllowReceive(this, item_to_hands, -1, false, true);
    }
}

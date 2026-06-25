// spacecat_havemax - container hooks
//
// ItemBase is the base for every item and container in DayZ. Overriding the
// receive hooks here covers items being dropped into ANY container that is part
// of a player's inventory (backpacks, vests, the contents of nested containers,
// weapon attachment slots, etc.). The logic resolves the owning player via the
// hierarchy root, so ground/world containers are left unrestricted.

modded class ItemBase
{
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
}

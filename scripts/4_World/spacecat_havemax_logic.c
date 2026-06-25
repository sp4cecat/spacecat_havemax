// spacecat_havemax - enforcement logic
//
// Central gatekeeper shared by every modded CanReceive* hook. Resolves the
// destination player, applies the per-class slot rule to the incoming item,
// then enforces the per-class count limit across the player's whole inventory
// hierarchy (hands + clothing + every nested container).
//
// Counting any container the player puts into hands/inventory is handled here
// for free: the "incoming tree" is the incoming item PLUS everything nested
// inside it, so picking up a backpack stuffed with restricted items is checked
// against the same limit as carrying them loose.

class SpacecatHaveMaxLogic
{
    static bool s_Debug = false;

    // A single inventory action calls the CanReceive* hooks more than once
    // (predictive + authoritative passes, plus drag-hover validation). Collapse
    // those repeats into one player-facing reaction within this window so a lone
    // failed attempt yields a single notification / log line, not a burst.
    static const int REACT_COOLDOWN_MS = 1500;

    // receiver  : the entity being asked to accept the item (container or player)
    // incoming  : the item being moved
    // slotId    : attachment slot id (>= 0 for attachment, ignored otherwise)
    // isCargo   : true when the item is going into cargo
    // isHands   : true when the item is going into the player's hands
    static bool AllowReceive(EntityAI receiver, EntityAI incoming, int slotId, bool isCargo, bool isHands)
    {
        // Server is the authority. Clients optimistically allow and let the
        // server's verdict bounce the item back if it disagrees.
        if (!GetGame() || !GetGame().IsServer())
            return true;

        if (!receiver || !incoming)
            return true;

        SpacecatHaveMaxConfig cfg = SpacecatHaveMaxConfig.GetInstance();
        if (!cfg.IsEnabled())
            return true;

        // Restriction is per-player-inventory; resolve the owning player.
        PlayerBase player = PlayerBase.Cast(receiver);
        if (!player)
            player = PlayerBase.Cast(receiver.GetHierarchyRootPlayer());
        if (!player)
            return true; // not destined for a player's inventory

        // ---------------------------------------------------------------
        // Slot rule - applies to the incoming item's OWN class only.
        // ---------------------------------------------------------------
        SpacecatHaveMaxEntry self = cfg.GetEntryForType(incoming.GetType());
        if (self && self.HasSlotRestriction() && !isHands)
        {
            if (isCargo)
                return Deny(player, incoming, self, "slot-restricted item cannot go in cargo");

            string slotName = InventorySlots.GetSlotName(slotId);
            if (!self.AllowsSlotName(slotName))
                return Deny(player, incoming, self, "slot '" + slotName + "' not in allowed list");
        }

        // ---------------------------------------------------------------
        // Count rule - total of each restricted class across the player.
        // ---------------------------------------------------------------

        // Everything arriving together: the incoming item + all nested contents.
        // EnumerateInventory(PREORDER) already includes the inventory's owner
        // (the incoming item) as the root element, so enumerate first and only
        // add `incoming` ourselves if the traversal didn't (items with no
        // sub-inventory). Inserting it unconditionally double-counts the root.
        array<EntityAI> incomingTree = new array<EntityAI>();
        if (incoming.GetInventory())
            incoming.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, incomingTree);
        if (incomingTree.Find(incoming) == -1)
            incomingTree.Insert(incoming);

        // Tally the incoming contribution per rule (keyed by the rule's class so
        // every subclass folds into the same limit).
        map<string, int> incomingCounts = new map<string, int>();
        for (int i = 0; i < incomingTree.Count(); i++)
        {
            SpacecatHaveMaxEntry e = cfg.GetEntryForType(incomingTree[i].GetType());
            if (e)
                BumpCount(incomingCounts, e.className, 1);
        }

        if (incomingCounts.Count() == 0)
            return true; // nothing restricted is arriving

        // Items already on the player (excluding the arriving tree so that
        // relocating items already owned never double-counts).
        array<EntityAI> playerItems = new array<EntityAI>();
        if (player.GetInventory())
            player.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, playerItems);

        for (int c = 0; c < incomingCounts.Count(); c++)
        {
            string ruleClass = incomingCounts.GetKey(c);
            int incomingN    = incomingCounts.GetElement(c);

            SpacecatHaveMaxEntry entry = cfg.GetEntryForType(ruleClass);
            if (!entry)
                continue;

            int current = 0;
            for (int o = 0; o < playerItems.Count(); o++)
            {
                EntityAI ownedItem = playerItems[o];
                if (incomingTree.Find(ownedItem) != -1)
                    continue; // part of the arriving tree, counted via incomingN

                SpacecatHaveMaxEntry oe = cfg.GetEntryForType(ownedItem.GetType());
                if (oe && oe.className == entry.className)
                    current++;
            }

            DebugLog(incoming.GetType() + " check: rule=" + entry.className + " current=" + current + " incoming=" + incomingN + " max=" + entry.max + " isCargo=" + isCargo + " isHands=" + isHands + " slotId=" + slotId);
            if (current + incomingN > entry.max)
                return Deny(player, incoming, entry, "limit reached (" + (current + incomingN) + " > max " + entry.max + ")");
        }

        return true;
    }

    // Always returns false (the caller's denial verdict). Side effects - logging
    // (when log_events) and the on-screen notification (when show_notifications
    // and the entry has notification text) - are throttled per player.
    private static bool Deny(PlayerBase player, EntityAI incoming, SpacecatHaveMaxEntry entry, string reason)
    {
        DebugLog(incoming.GetType() + " DENIED: rule=" + entry.className + " (" + reason + ")");

        SpacecatHaveMaxConfig cfg = SpacecatHaveMaxConfig.GetInstance();
        if (!cfg.LogEvents() && !cfg.ShowNotifications())
            return false;

        int now = GetGame().GetTime();
        if (now - player.GetHaveMaxLastReact() < REACT_COOLDOWN_MS)
            return false; // collapse repeat checks from the same action
        player.SetHaveMaxLastReact(now);

        if (cfg.LogEvents())
        {
            string who = "player";
            if (player.GetIdentity())
                who = player.GetIdentity().GetName() + " (" + player.GetIdentity().GetId() + ")";
            Print("[spacecat_havemax] BLOCKED: " + who + " could not add " + incoming.GetType() + " - " + reason + " [rule " + entry.className + ", max " + entry.max + "]");
        }

        if (cfg.ShowNotifications() && entry.notification != string.Empty)
            player.MessageImportant(entry.notification);

        return false;
    }

    private static void BumpCount(map<string, int> counts, string key, int by)
    {
        int v = 0;
        counts.Find(key, v);
        counts.Set(key, v + by);
    }

    private static void DebugLog(string msg)
    {
        if (s_Debug)
            Print("[spacecat_havemax] " + msg);
    }
}

// SPDX-License-Identifier: MIT
// IAR loads every address from a literal pool next to the code. Types each pool word that code reads and whose value
// lands in a non-code block (RAM, peripheral, EEPROM, bit-band) as a pointer, so the decompiler, with the image
// read-only, folds the load into the target: GPIOG.IDR instead of *DAT_08015408. Words pointing into the image are
// left alone (they may be code addresses or constant data, typed by analysis). Args: none
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;

public class TypePoolPointers extends GhidraScript {
    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        MemoryBlock img = mem.getBlock(toAddr(0x08006000L));
        ReferenceManager rm = currentProgram.getReferenceManager();
        Listing listing = currentProgram.getListing();
        int typed = 0, skipped = 0;
        for (Address a = img.getStart(); a.compareTo(img.getEnd().subtract(3)) <= 0; a = a.add(4)) {
            if (listing.getInstructionContaining(a) != null) continue;
            boolean read = false;
            for (Reference r : rm.getReferencesTo(a)) if (r.getReferenceType().isRead()) { read = true; break; }
            if (!read) continue;
            long v = mem.getInt(a) & 0xffffffffL;
            Address t;
            try { t = toAddr(v); } catch (Exception e) { continue; }
            MemoryBlock tb = mem.getBlock(t);
            if (tb == null || tb == img) continue;
            Data d = listing.getDataAt(a);
            if (d != null && d.isDefined() && !(d.getDataType() instanceof Undefined)) { skipped++; continue; }
            try {
                listing.clearCodeUnits(a, a.add(3), false);
                listing.createData(a, PointerDataType.dataType);
                rm.addMemoryReference(a, t, RefType.DATA, SourceType.ANALYSIS, 0);
                typed++;
            } catch (Exception e) { skipped++; }
        }
        println("TypePoolPointers: " + typed + " pool words typed as pointers, " + skipped + " left as they were");
    }
}

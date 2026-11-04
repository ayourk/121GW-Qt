// SPDX-License-Identifier: MIT
// Marks the loaded image (the block holding the vector table) read + execute, not write. The raw-binary loader makes
// it writable, so the decompiler treats IAR's literal-pool words as variables (*DAT_08015408) instead of constants;
// read-only, it follows them to the register or RAM they point at (GPIOG.IDR). Args: [base, default 0x08006000]
import ghidra.app.script.GhidraScript;
import ghidra.program.model.mem.*;

public class FlashReadOnly extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        long base = args.length > 0 ? Long.decode(args[0]) : 0x08006000L;
        MemoryBlock b = currentProgram.getMemory().getBlock(toAddr(base));
        b.setWrite(false); b.setExecute(true); b.setRead(true);
        b.setName("FLASH_APP");
        println("FlashReadOnly: " + b.getName() + " " + b.getStart() + "-" + b.getEnd() + " now r-x");
    }
}

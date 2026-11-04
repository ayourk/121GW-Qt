// SPDX-License-Identifier: MIT
// Prints the disassembly of the functions at the given addresses (one line per instruction, with any reference's
// symbol), for reading what the decompiler hides (stack structures built byte by byte). Args: addr [addr ...]
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;

public class DumpAsm extends GhidraScript {
    @Override
    public void run() throws Exception {
        for (String s : getScriptArgs()) {
            Function f = getFunctionAt(toAddr(Long.decode("0x" + s.replaceFirst("^0x", ""))));
            if (f == null) { println("no function at " + s); continue; }
            StringBuilder out = new StringBuilder("\n== " + f.getName() + " @ " + f.getEntryPoint());
            for (Instruction i : currentProgram.getListing().getInstructions(f.getBody(), true)) {
                out.append("\n").append(i.getAddress()).append("  ").append(i);
                for (Reference r : i.getReferencesFrom()) {
                    Symbol sym = getSymbolAt(r.getToAddress());
                    if (sym != null) out.append("   ; ").append(sym.getName());
                }
            }
            println(out.toString());
        }
    }
}

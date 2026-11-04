// SPDX-License-Identifier: MIT
// Writes every function's decompiled C (ordered by address) and a tab-separated index: entry, name, body size,
// callers, callees, the named peripherals it reaches (from ApplySvd's labels) and whether decompiling succeeded.
// Args: out.c out-index.tsv
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class ExportDecomp extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        DecompInterface d = new DecompInterface();
        d.toggleCCode(true); d.toggleSyntaxTree(false);
        DecompileOptions o = new DecompileOptions();
        o.grabFromProgram(currentProgram);
        o.setRespectReadOnly(true);   // fold IAR's literal-pool loads (the image is read-only, FlashReadOnly.java)
        d.setOptions(o);
        d.openProgram(currentProgram);
        Memory mem = currentProgram.getMemory();
        ReferenceManager rm = currentProgram.getReferenceManager();
        int ok = 0, failed = 0, n = 0;
        try (PrintWriter c = new PrintWriter(new FileWriter(args[0])); PrintWriter x = new PrintWriter(new FileWriter(args[1]))) {
            x.println("entry\tname\tbytes\tcallers\tcallees\tperipherals\tdecompiled");
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                n++;
                TreeSet<String> per = new TreeSet<>();
                for (Address a : f.getBody().getAddresses(true)) {
                    for (Reference r : rm.getReferencesFrom(a)) {
                        MemoryBlock b = mem.getBlock(r.getToAddress());
                        if (b != null && b.isVolatile()) {
                            Symbol s = getSymbolAt(r.getToAddress());
                            Data dd = getDataContaining(r.getToAddress());
                            String nm = b.getName();
                            if (dd != null && dd.getPrimarySymbol() != null) nm = dd.getPrimarySymbol().getName();
                            else if (s != null) nm = s.getName().replaceAll("_.*", "");
                            per.add(nm);
                        }
                    }
                }
                DecompileResults r = d.decompileFunction(f, 120, monitor);
                boolean good = r.decompileCompleted();
                if (good) ok++; else failed++;
                c.println("// ==== " + f.getName() + " @ " + f.getEntryPoint() + (per.isEmpty() ? "" : "  [" + String.join(" ", per) + "]"));
                c.println(good ? r.getDecompiledFunction().getC() : "// FAILED " + r.getErrorMessage());
                x.println(f.getEntryPoint() + "\t" + f.getName() + "\t" + f.getBody().getNumAddresses() + "\t"
                        + f.getSymbol().getReferenceCount() + "\t" + f.getCalledFunctions(monitor).size() + "\t"
                        + String.join(" ", per) + "\t" + good);
            }
        }
        d.dispose();
        println("ExportDecomp: " + n + " functions, " + ok + " decompiled, " + failed + " failed");
    }
}

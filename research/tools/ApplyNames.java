// SPDX-License-Identifier: MIT
// Applies the hand-kept names file: one line per function or global, tab-separated
//   address  name  flags  comment
// flags: f = function (created if missing), n = no-return, g = global (label only), - = none. The comment becomes
// the function's plate comment (or the global's EOL comment). Lines starting with # are skipped. Args: names.tsv
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.nio.file.*;

public class ApplyNames extends GhidraScript {
    @Override
    public void run() throws Exception {
        int fn = 0, gl = 0, bad = 0;
        for (String line : Files.readAllLines(Path.of(getScriptArgs()[0]))) {
            if (line.isBlank() || line.startsWith("#")) continue;
            String[] c = line.split("\t", 4);
            if (c.length < 3) { println("bad line: " + line); bad++; continue; }
            Address a = toAddr(Long.decode("0x" + c[0].replaceFirst("^0x", "")));
            String name = c[1], flags = c[2], comment = c.length > 3 ? c[3].trim() : "";
            if (flags.contains("g")) {
                currentProgram.getSymbolTable().createLabel(a, name, SourceType.USER_DEFINED).setPrimary();
                if (!comment.isEmpty()) setEOLComment(a, comment);
                gl++;
                continue;
            }
            Function f = getFunctionAt(a);
            if (f == null && flags.contains("f")) { disassemble(a); f = createFunction(a, name); }
            if (f == null) { println("no function at " + a + " for " + name); bad++; continue; }
            f.setName(name, SourceType.USER_DEFINED);
            if (flags.contains("n")) f.setNoReturn(true);
            if (!comment.isEmpty()) f.setComment(comment);
            fn++;
        }
        println("ApplyNames: " + fn + " functions, " + gl + " globals, " + bad + " problems");
    }
}

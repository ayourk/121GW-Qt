// SPDX-License-Identifier: MIT
// Types the Cortex-M vector table at the image base and names every handler: the core exceptions by their ARM names,
// the device interrupts from the SVD's <interrupt> entries (vector = 16 + value). Each handler becomes a function
// (Thumb bit dropped), so analysis starts from all of them. Handlers that share an address (default handlers) get one
// name plus a comment listing the rest. Args: path/to/device.svd [base, default 0x08006000]
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import javax.xml.parsers.*;
import org.w3c.dom.*;
import java.io.File;
import java.util.*;

public class NameVectors extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        long base = args.length > 1 ? Long.decode(args[1]) : 0x08006000L;
        Document doc = DocumentBuilderFactory.newInstance().newDocumentBuilder().parse(new File(args[0]));
        Map<Integer, String> names = new TreeMap<>();
        String[] core = {"__initial_sp", "Reset_Handler", "NMI_Handler", "HardFault_Handler", "MemManage_Handler",
                "BusFault_Handler", "UsageFault_Handler", null, null, null, null, "SVC_Handler", "DebugMon_Handler", null,
                "PendSV_Handler", "SysTick_Handler"};
        for (int i = 0; i < core.length; i++) if (core[i] != null) names.put(i, core[i]);
        NodeList ints = doc.getElementsByTagName("interrupt");
        int maxV = 15;
        for (int i = 0; i < ints.getLength(); i++) {
            Element e = (Element) ints.item(i);
            String n = e.getElementsByTagName("name").item(0).getTextContent().trim();
            int v = Integer.parseInt(e.getElementsByTagName("value").item(0).getTextContent().trim()) + 16;
            names.putIfAbsent(v, n + "_IRQHandler");
            maxV = Math.max(maxV, v);
        }
        Listing listing = currentProgram.getListing();
        SymbolTable st = currentProgram.getSymbolTable();
        Address tab = toAddr(base);
        int count = maxV + 1;
        listing.clearCodeUnits(tab, tab.add(count * 4L - 1), false);
        createData(tab, new ArrayDataType(PointerDataType.dataType, count, 4));
        st.createLabel(tab, "vector_table", SourceType.USER_DEFINED);
        Map<Long, List<String>> byTarget = new LinkedHashMap<>();
        for (int i = 1; i < count; i++) {
            long v = getInt(tab.add(i * 4L)) & 0xffffffffL;
            if (v == 0 || v == 0xffffffffL) continue;
            String n = names.getOrDefault(i, "Vector" + i + "_Handler");
            byTarget.computeIfAbsent(v & ~1L, k -> new ArrayList<>()).add(n);
        }
        int made = 0, shared = 0;
        for (Map.Entry<Long, List<String>> en : byTarget.entrySet()) {
            Address a = toAddr(en.getKey());
            if (!currentProgram.getMemory().contains(a)) { println("vector target outside the image: " + a + " " + en.getValue()); continue; }
            disassemble(a);
            Function f = getFunctionAt(a);
            if (f == null) f = createFunction(a, null);
            List<String> ns = en.getValue();
            String name = ns.size() == 1 ? ns.get(0) : (ns.contains("Reset_Handler") ? "Reset_Handler" : "Default_Handler");
            if (f != null) f.setName(name, SourceType.USER_DEFINED); else st.createLabel(a, name, SourceType.USER_DEFINED);
            if (ns.size() > 1) { setPlateComment(a, "Vector target of: " + String.join(", ", ns)); shared++; }
            made++;
        }
        long sp = getInt(tab) & 0xffffffffL;
        println("NameVectors: " + count + " vectors, " + made + " handler addresses (" + shared + " shared), initial SP 0x"
                + Long.toHexString(sp));
    }
}

// SPDX-License-Identifier: MIT
// Applies a CMSIS SVD to a raw Cortex-M image: one volatile memory block per peripheral, a structure type per
// peripheral (each register at its offset, its fields listed in the register's comment), applied at the base address
// under the peripheral's name, so the decompiler reads GPIOG.IDR instead of *(uint *)0x40021c10. Also adds the memory
// a raw image lacks (RAM, bit-band aliases, data EEPROM, the bootloader below the app, Cortex-M3 SysTick and SCB).
// Args: path/to/device.svd
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import javax.xml.parsers.*;
import org.w3c.dom.*;
import java.io.File;
import java.util.*;

public class ApplySvd extends GhidraScript {
    private final CategoryPath cat = new CategoryPath("/SVD");

    private static String text(Element e, String tag) {
        NodeList l = e.getChildNodes();
        for (int i = 0; i < l.getLength(); i++)
            if (l.item(i) instanceof Element c && c.getTagName().equals(tag)) return c.getTextContent().trim();
        return null;
    }
    private static Element child(Element e, String tag) {
        NodeList l = e.getChildNodes();
        for (int i = 0; i < l.getLength(); i++)
            if (l.item(i) instanceof Element c && c.getTagName().equals(tag)) return c;
        return null;
    }
    private static List<Element> children(Element e, String tag) {
        List<Element> out = new ArrayList<>();
        if (e == null) return out;
        NodeList l = e.getChildNodes();
        for (int i = 0; i < l.getLength(); i++)
            if (l.item(i) instanceof Element c && c.getTagName().equals(tag)) out.add(c);
        return out;
    }
    private static long num(String s) {
        s = s.trim().toLowerCase();
        return s.startsWith("0x") ? Long.parseLong(s.substring(2), 16) : Long.parseLong(s);
    }

    /** Creates the block; when the range overlaps one already there, makes none and returns how many bytes from
     *  start the existing memory holds (0 = none), so the caller can fit its structure into it. */
    private long block(String name, long start, long len, boolean volatil) throws Exception {
        Memory mem = currentProgram.getMemory();
        Address a = toAddr(start);
        MemoryBlock at = mem.getBlock(a);
        if (at != null || mem.getBlock(a.add(len - 1)) != null) {
            long room = at != null ? Math.min(len, at.getEnd().subtract(a) + 1) : 0;
            println("block " + name + " overlaps " + (at != null ? at.getName() : "a later block") + ": fitting " + room + " bytes");
            return room;
        }
        MemoryBlock b = mem.createUninitializedBlock(name, a, len, false);
        b.setRead(true); b.setWrite(true); b.setExecute(false); b.setVolatile(volatil);
        return len;
    }

    private int limit;   // bytes the current peripheral's structure may span

    private void place(Structure s, Element reg, int offset, String name, long regSize) {
        DataType t = switch ((int) regSize) { case 8 -> ByteDataType.dataType; case 16 -> WordDataType.dataType; default -> DWordDataType.dataType; };
        int n = t.getLength();
        if (offset + n > limit) { println("  " + s.getName() + "." + name + " beyond the memory it fits in: dropped"); return; }
        if (offset + n > s.getLength()) s.growStructure(offset + n - s.getLength());
        DataTypeComponent c = s.getComponentAt(offset);
        if (c != null && c.getDataType() != DataType.DEFAULT) return;   // an overlapping alternate view: first one wins
        StringBuilder fields = new StringBuilder();
        String desc = text(reg, "description");
        if (desc != null) fields.append(desc.replaceAll("\\s+", " "));
        for (Element f : children(child(reg, "fields"), "field")) {
            String bo = text(f, "bitOffset"), bw = text(f, "bitWidth");
            if (fields.length() > 0) fields.append(fields.length() > 0 && fields.indexOf("|") < 0 ? " | " : ", ");
            fields.append(text(f, "name"));
            if (bo != null) fields.append("[").append(bo).append(bw != null && !bw.equals("1") ? "+" + bw : "").append("]");
        }
        try { s.replaceAtOffset(offset, t, n, name, fields.toString()); } catch (Exception e) { println("  " + s.getName() + "." + name + ": " + e.getMessage()); }
    }

    @Override
    public void run() throws Exception {
        String path = getScriptArgs()[0];
        Document doc = DocumentBuilderFactory.newInstance().newDocumentBuilder().parse(new File(path));
        Element dev = doc.getDocumentElement();
        long devSize = num(Objects.requireNonNullElse(text(dev, "size"), "32"));
        Map<String, Element> byName = new HashMap<>();
        List<Element> periphs = children(child(dev, "peripherals"), "peripheral");
        for (Element p : periphs) byName.put(text(p, "name"), p);
        DataTypeManager dtm = currentProgram.getDataTypeManager();
        SymbolTable st = currentProgram.getSymbolTable();
        Listing listing = currentProgram.getListing();
        int nP = 0, nR = 0;

        // Memory the raw image does not carry (STM32L152ZD: 384 KB flash, 48 KB RAM, 12 KB data EEPROM).
        block("BOOTLOADER", 0x08000000L, 0x6000, false);
        block("RAM", 0x20000000L, 0xC000, false);
        block("RAM_BITBAND", 0x22000000L, 0xC000L * 32, false);
        block("PERIPH_BITBAND", 0x42000000L, 0x2000000L, true);
        block("DATA_EEPROM", 0x08080000L, 0x3000, false);
        block("INFO_OPTION_BYTES", 0x1FF80000L, 0x100, true);
        block("SYSTEM_MEMORY", 0x1FF00000L, 0x1000, false);

        // Pass 1: every peripheral's range (SVD, plus the Cortex-M3 SysTick and SCB the SVD leaves out).
        record Per(String name, long base, long size, Element src, Element self, String[] coreRegs) {}
        List<Per> all = new ArrayList<>();
        for (Element p : periphs) {
            Element src = p;
            String derived = p.getAttribute("derivedFrom");
            if (!derived.isEmpty() && byName.containsKey(derived)) src = byName.get(derived);
            long base = num(text(p, "baseAddress"));
            Element ab = child(p, "addressBlock");
            if (ab == null) ab = child(src, "addressBlock");
            long size = ab != null ? num(text(ab, "size")) : 0x400;
            if (ab != null && text(ab, "offset") != null) base += num(text(ab, "offset"));
            all.add(new Per(text(p, "name"), base, size, src, p, null));
        }
        all.add(new Per("SYSTICK", 0xE000E010L, 16, null, null, "CTRL,LOAD,VAL,CALIB".split(",")));
        all.add(new Per("SCB", 0xE000ED00L, 64, null, null,
                "CPUID,ICSR,VTOR,AIRCR,SCR,CCR,SHPR1,SHPR2,SHPR3,SHCSR,CFSR,HFSR,DFSR,MMFAR,BFAR,AFSR".split(",")));

        // Memory: the union of the ranges, one volatile block per merged run (named after its first peripheral).
        List<Per> sorted = new ArrayList<>(all);
        sorted.sort(Comparator.comparingLong(Per::base));
        long runStart = -1, runEnd = -1; String runName = null;
        for (Per q : sorted) {
            if (runStart >= 0 && q.base() <= runEnd) { runEnd = Math.max(runEnd, q.base() + q.size()); continue; }
            if (runStart >= 0) block(runName, runStart, runEnd - runStart, true);
            runStart = q.base(); runEnd = q.base() + q.size(); runName = q.name();
        }
        if (runStart >= 0) block(runName, runStart, runEnd - runStart, true);

        // Pass 2: a peripheral overlapping no other gets a structure; an overlapping one gets each register labeled.
        int nS = 0, nL = 0;
        for (Per q : all) {
            boolean overlaps = false;
            for (Per o : all)
                if (o != q && q.base() < o.base() + o.size() && o.base() < q.base() + q.size()) { overlaps = true; break; }
            // (name, offset, bits, element) of every register
            List<Object[]> regs = new ArrayList<>();
            if (q.coreRegs() != null) {
                for (int i = 0; i < q.coreRegs().length; i++) regs.add(new Object[]{q.coreRegs()[i], (long) i * 4, 32L, null});
            } else {
                for (Element r : children(child(q.src(), "registers"), "register")) {
                    long off = num(text(r, "addressOffset"));
                    long rs = text(r, "size") != null ? num(text(r, "size")) : devSize;
                    String rn = text(r, "name");
                    String dim = text(r, "dim");
                    if (dim != null) {
                        long inc = num(text(r, "dimIncrement"));
                        String[] idx = text(r, "dimIndex") != null ? text(r, "dimIndex").split(",") : null;
                        for (int i = 0; i < num(dim); i++) {
                            String sub = idx != null && i < idx.length ? idx[i].trim() : Integer.toString(i);
                            regs.add(new Object[]{rn.replace("[%s]", sub).replace("%s", sub), off + i * inc, rs, r});
                        }
                    } else regs.add(new Object[]{rn, off, rs, r});
                }
            }
            Address a = toAddr(q.base());
            if (!overlaps) {
                limit = (int) q.size();
                Structure s = new StructureDataType(cat, q.name() + "_t", (int) q.size(), dtm);
                for (Object[] r : regs) {
                    if (r[3] != null) place(s, (Element) r[3], (int) (long) (Long) r[1], (String) r[0], (Long) r[2]);
                    else s.replaceAtOffset((int) (long) (Long) r[1], DWordDataType.dataType, 4, (String) r[0], null);
                    nR++;
                }
                DataType applied = dtm.addDataType(s, DataTypeConflictHandler.REPLACE_HANDLER);
                listing.clearCodeUnits(a, a.add(q.size() - 1), false);
                listing.createData(a, applied);
                nS++;
            } else {
                for (Object[] r : regs) {
                    long rs = (Long) r[2];
                    DataType t = rs == 16 ? WordDataType.dataType : rs == 8 ? ByteDataType.dataType : DWordDataType.dataType;
                    Address ra = a.add((Long) r[1]);
                    if (listing.getDefinedDataAt(ra) == null) {
                        try { listing.createData(ra, t); } catch (Exception e) { println("  " + q.name() + "_" + r[0] + ": " + e.getMessage()); }
                    }
                    st.createLabel(ra, q.name() + "_" + r[0], SourceType.IMPORTED);
                    if (r[3] != null) {
                        String d = text((Element) r[3], "description");
                        if (d != null) listing.setComment(ra, CodeUnit.EOL_COMMENT, d.replaceAll("\\s+", " "));
                    }
                    nR++;
                }
                nL++;
            }
            st.createLabel(a, q.name(), SourceType.IMPORTED);
            String d = q.self() != null ? text(q.self(), "description") : "Cortex-M3 " + q.name();
            if (d != null) listing.setComment(a, CodeUnit.PLATE_COMMENT, q.name() + ": " + d.replaceAll("\\s+", " "));
            nP++;
        }
        println("ApplySvd: " + nP + " peripherals (" + nS + " as structures, " + nL + " overlapping ones as labeled registers), "
                + nR + " registers from " + path);
    }
}

// Writes the bytes of every initialized memory block of a Ghidra program to
// <out dir>/<block>_<rva>.bin, for comparing dumps outside Ghidra. Read-only.
//
// analyzeHeadless <project dir> <project> -process <program> -readOnly
//     -noanalysis -scriptPath tools/re/ghidra -postScript ExportMemory.java <out dir>
//@category CMFM

import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStream;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.MemoryBlock;

public class ExportMemory extends GhidraScript {

	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length < 1) {
			printerr("usage: ExportMemory.java <out dir>");
			return;
		}
		File out = new File(args[0]);
		out.mkdirs();
		Address base = currentProgram.getImageBase();
		byte[] buf = new byte[1 << 20];
		for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
			if (!b.isInitialized()) {
				continue;
			}
			long rva = b.getStart().subtract(base);
			String name = b.getName().replaceAll("[^A-Za-z0-9_]", "") + "_" + Long.toHexString(rva) + ".bin";
			try (OutputStream os = new FileOutputStream(new File(out, name))) {
				long left = b.getSize();
				Address a = b.getStart();
				while (left > 0) {
					monitor.checkCancelled();
					int n = (int) Math.min(buf.length, left);
					int got = b.getBytes(a, buf, 0, n);
					os.write(buf, 0, got);
					left -= got;
					a = a.add(got);
				}
			}
			println("ExportMemory: " + name + " " + Long.toHexString(b.getSize()));
		}
	}
}

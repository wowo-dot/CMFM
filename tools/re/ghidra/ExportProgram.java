// Exports what an annotated Ghidra program knows, as UTF-8 TSV files, for
// use from IDA and Python. Read-only: changes nothing in the program.
//
// analyzeHeadless <project dir> <project> -process <program> -readOnly
//     -noanalysis -scriptPath tools/re/ghidra -postScript ExportProgram.java <out dir>
//
// Output (<out dir>): info.txt, blocks.tsv, functions.tsv, labels.tsv,
// structs.tsv, comments.tsv. Addresses are RVAs from the image base, in hex.
//@category CMFM

import java.io.BufferedWriter;
import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

import ghidra.app.script.GhidraScript;
import ghidra.framework.options.Options;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressIterator;
import ghidra.program.model.data.DataTypeComponent;
import ghidra.program.model.data.DataTypeManager;
import ghidra.program.model.data.Structure;
import ghidra.program.model.listing.CodeUnit;
import ghidra.program.model.listing.CommentType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.listing.Program;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import ghidra.program.model.symbol.SymbolType;

public class ExportProgram extends GhidraScript {

	private Address base;

	private static String clean(String s) {
		if (s == null) {
			return "";
		}
		return s.replace("\\", "\\\\").replace("\t", "\\t").replace("\r", "").replace("\n", "\\n");
	}

	private String rva(Address a) {
		return Long.toHexString(a.subtract(base));
	}

	private BufferedWriter open(File dir, String name) throws IOException {
		return Files.newBufferedWriter(new File(dir, name).toPath(), StandardCharsets.UTF_8);
	}

	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length < 1) {
			printerr("usage: ExportProgram.java <out dir>");
			return;
		}
		File out = new File(args[0]);
		out.mkdirs();
		Program p = currentProgram;
		base = p.getImageBase();

		try (BufferedWriter w = open(out, "info.txt")) {
			w.write("name\t" + p.getName() + "\n");
			w.write("executable_path\t" + p.getExecutablePath() + "\n");
			w.write("executable_format\t" + p.getExecutableFormat() + "\n");
			w.write("executable_md5\t" + p.getExecutableMD5() + "\n");
			w.write("executable_sha256\t" + p.getExecutableSHA256() + "\n");
			w.write("image_base\t" + base + "\n");
			w.write("language\t" + p.getLanguageID() + "\n");
			w.write("compiler\t" + p.getCompilerSpec().getCompilerSpecID() + "\n");
			w.write("creation_date\t" + p.getCreationDate() + "\n");
			Options info = p.getOptions(Program.PROGRAM_INFO);
			for (String n : info.getOptionNames()) {
				w.write("info:" + clean(n) + "\t" + clean(info.getValueAsString(n)) + "\n");
			}
		}

		try (BufferedWriter w = open(out, "blocks.tsv")) {
			w.write("name\trva\tsize\tperm\tinitialized\n");
			for (MemoryBlock b : p.getMemory().getBlocks()) {
				w.write(clean(b.getName()) + "\t" + rva(b.getStart()) + "\t" + Long.toHexString(b.getSize()) + "\t"
					+ (b.isRead() ? "r" : "-") + (b.isWrite() ? "w" : "-") + (b.isExecute() ? "x" : "-") + "\t"
					+ b.isInitialized() + "\n");
			}
		}

		int total = 0;
		int named = 0;
		try (BufferedWriter w = open(out, "functions.tsv")) {
			w.write("rva\tsource\tname\tprototype\tbytes16\n");
			for (Function f : p.getFunctionManager().getFunctions(true)) {
				monitor.checkCancelled();
				total++;
				Symbol s = f.getSymbol();
				if (s.getSource() == SourceType.DEFAULT) {
					continue;
				}
				named++;
				String bytes = "";
				try {
					byte[] b = getBytes(f.getEntryPoint(), 16);
					StringBuilder sb = new StringBuilder();
					for (byte x : b) {
						sb.append(String.format("%02x", x & 0xff));
					}
					bytes = sb.toString();
				}
				catch (Exception e) {
					bytes = "?";
				}
				w.write(rva(f.getEntryPoint()) + "\t" + s.getSource() + "\t" + clean(s.getName(true)) + "\t"
					+ clean(f.getPrototypeString(false, true)) + "\t" + bytes + "\n");
			}
		}

		int labels = 0;
		try (BufferedWriter w = open(out, "labels.tsv")) {
			w.write("rva\tsource\tname\n");
			SymbolIterator it = p.getSymbolTable().getAllSymbols(false);
			while (it.hasNext()) {
				monitor.checkCancelled();
				Symbol s = it.next();
				if (s.isExternal() || s.getSymbolType() != SymbolType.LABEL || s.getSource() == SourceType.DEFAULT) {
					continue;
				}
				labels++;
				w.write(rva(s.getAddress()) + "\t" + s.getSource() + "\t" + clean(s.getName(true)) + "\n");
			}
		}

		int structs = 0;
		DataTypeManager dtm = p.getDataTypeManager();
		try (BufferedWriter w = open(out, "structs.tsv")) {
			w.write("struct\tsize\toffset\tlength\tfield\ttype\tcomment\n");
			java.util.Iterator<Structure> it = dtm.getAllStructures();
			while (it.hasNext()) {
				monitor.checkCancelled();
				Structure st = it.next();
				structs++;
				String path = clean(st.getPathName());
				w.write(path + "\t" + Integer.toHexString(st.getLength()) + "\t\t\t\t\t" + clean(st.getDescription()) + "\n");
				for (DataTypeComponent c : st.getDefinedComponents()) {
					w.write(path + "\t\t" + Integer.toHexString(c.getOffset()) + "\t" + Integer.toHexString(c.getLength())
						+ "\t" + clean(c.getFieldName()) + "\t" + clean(c.getDataType().getDisplayName()) + "\t"
						+ clean(c.getComment()) + "\n");
				}
			}
		}

		int comments = 0;
		Listing listing = p.getListing();
		CommentType[] kinds = { CommentType.PLATE, CommentType.PRE, CommentType.EOL, CommentType.POST,
			CommentType.REPEATABLE };
		try (BufferedWriter w = open(out, "comments.tsv")) {
			w.write("rva\tkind\ttext\n");
			AddressIterator it = listing.getCommentAddressIterator(p.getMemory(), true);
			while (it.hasNext()) {
				monitor.checkCancelled();
				Address a = it.next();
				CodeUnit cu = listing.getCodeUnitAt(a);
				if (cu == null) {
					continue;
				}
				for (CommentType k : kinds) {
					String c = cu.getComment(k);
					if (c != null) {
						comments++;
						w.write(rva(a) + "\t" + k + "\t" + clean(c) + "\n");
					}
				}
			}
		}

		try (BufferedWriter w = Files.newBufferedWriter(new File(out, "info.txt").toPath(), StandardCharsets.UTF_8,
			java.nio.file.StandardOpenOption.APPEND)) {
			w.write("functions_total\t" + total + "\n");
			w.write("functions_named\t" + named + "\n");
			w.write("labels_named\t" + labels + "\n");
			w.write("structures\t" + structs + "\n");
			w.write("datatypes\t" + dtm.getDataTypeCount(true) + "\n");
			w.write("comments\t" + comments + "\n");
		}
		println("ExportProgram: " + named + "/" + total + " named functions, " + labels + " labels, " + structs
			+ " structs, " + comments + " comments -> " + out);
	}
}

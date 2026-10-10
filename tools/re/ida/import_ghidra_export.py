# Applies names and comments from tools/re/ghidra/ExportProgram.java output to
# the open IDA database. Run inside IDA (execute_python or File > Script):
#
#   exec(open(r"tools/re/ida/import_ghidra_export.py").read())
#   run(r"local/ghidra/export", tag="Fexty")
#
# Only places IDA has not named yet get a name (imports, exports and earlier
# user names are kept). Ghidra's automatic labels (switchD_ etc., source
# ANALYSIS) are skipped. Comments are added with a "[tag]" prefix and never
# replace an existing comment. Returns a dict of counts.

import os

import ida_bytes
import ida_funcs
import ida_name
import ida_nalt
import idc


def _read_tsv(path):
    with open(path, encoding="utf-8") as f:
        header = f.readline().rstrip("\n").split("\t")
        for line in f:
            yield dict(zip(header, line.rstrip("\n").split("\t")))


def _unescape(s):
    return s.replace("\\n", "\n").replace("\\t", "\t").replace("\\\\", "\\")


def _ida_name(name):
    # IDA keeps "::" in names; other characters it rejects become "_".
    fixed = ida_name.validate_name(name, ida_name.VNT_IDENT)
    return fixed or None


def _set_name(ea, name, counts):
    if ida_bytes.has_user_name(ida_bytes.get_flags(ea)):
        counts["kept_existing"] += 1
        return
    n = _ida_name(name)
    if not n:
        counts["bad_name"] += 1
        return
    if ida_name.set_name(ea, n, ida_name.SN_NOWARN | ida_name.SN_FORCE):
        counts["named"] += 1
    else:
        counts["failed"] += 1


def run(export_dir, tag="Ghidra"):
    base = ida_nalt.get_imagebase()
    counts = {k: 0 for k in ("named", "kept_existing", "bad_name", "failed", "func_created",
                             "func_missing", "labels_skipped_auto", "comments")}

    for row in _read_tsv(os.path.join(export_dir, "functions.tsv")):
        ea = base + int(row["rva"], 16)
        if ida_funcs.get_func(ea) is None or ida_funcs.get_func(ea).start_ea != ea:
            if ida_funcs.add_func(ea):
                counts["func_created"] += 1
            else:
                counts["func_missing"] += 1
        _set_name(ea, row["name"], counts)

    for row in _read_tsv(os.path.join(export_dir, "labels.tsv")):
        if row["source"] == "ANALYSIS":
            counts["labels_skipped_auto"] += 1
            continue
        _set_name(base + int(row["rva"], 16), row["name"], counts)

    for row in _read_tsv(os.path.join(export_dir, "comments.tsv")):
        ea = base + int(row["rva"], 16)
        text = "[%s %s] %s" % (tag, row["kind"].lower(), _unescape(row["text"]))
        f = ida_funcs.get_func(ea)
        if row["kind"] == "PLATE" and f is not None and f.start_ea == ea:
            old = idc.get_func_cmt(ea, 0) or ""
            idc.set_func_cmt(ea, (old + "\n" if old else "") + text, 0)
        else:
            old = idc.get_cmt(ea, 0) or ""
            idc.set_cmt(ea, (old + "\n" if old else "") + text, 0)
        counts["comments"] += 1

    return counts

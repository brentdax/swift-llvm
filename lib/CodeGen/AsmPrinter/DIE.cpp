//===--- lib/CodeGen/DIE.cpp - DWARF Info Entries -------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Data structures for DWARF info entries.
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/DIE.h"
#include "DwarfCompileUnit.h"
#include "DwarfDebug.h"
#include "DwarfUnit.h"
#include "llvm/ADT/Twine.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/LEB128.h"
#include "llvm/Support/MD5.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

//===----------------------------------------------------------------------===//
// DIEAbbrevData Implementation
//===----------------------------------------------------------------------===//

/// Profile - Used to gather unique data for the abbreviation folding set.
///
void DIEAbbrevData::Profile(FoldingSetNodeID &ID) const {
  // Explicitly cast to an integer type for which FoldingSetNodeID has
  // overloads.  Otherwise MSVC 2010 thinks this call is ambiguous.
  ID.AddInteger(unsigned(Attribute));
  ID.AddInteger(unsigned(Form));
}

//===----------------------------------------------------------------------===//
// DIEAbbrev Implementation
//===----------------------------------------------------------------------===//

/// Profile - Used to gather unique data for the abbreviation folding set.
///
void DIEAbbrev::Profile(FoldingSetNodeID &ID) const {
  ID.AddInteger(unsigned(Tag));
  ID.AddInteger(unsigned(Children));

  // For each attribute description.
  for (unsigned i = 0, N = Data.size(); i < N; ++i)
    Data[i].Profile(ID);
}

/// Emit - Print the abbreviation using the specified asm printer.
///
void DIEAbbrev::Emit(const AsmPrinter *AP) const {
  // Emit its Dwarf tag type.
  AP->EmitULEB128(Tag, dwarf::TagString(Tag).data());

  // Emit whether it has children DIEs.
  AP->EmitULEB128((unsigned)Children, dwarf::ChildrenString(Children).data());

  // For each attribute description.
  for (unsigned i = 0, N = Data.size(); i < N; ++i) {
    const DIEAbbrevData &AttrData = Data[i];

    // Emit attribute type.
    AP->EmitULEB128(AttrData.getAttribute(),
                    dwarf::AttributeString(AttrData.getAttribute()).data());

    // Emit form type.
    AP->EmitULEB128(AttrData.getForm(),
                    dwarf::FormEncodingString(AttrData.getForm()).data());
  }

  // Mark end of abbreviation.
  AP->EmitULEB128(0, "EOM(1)");
  AP->EmitULEB128(0, "EOM(2)");
}

LLVM_DUMP_METHOD
void DIEAbbrev::print(raw_ostream &O) {
  O << "Abbreviation @"
    << format("0x%lx", (long)(intptr_t)this)
    << "  "
    << dwarf::TagString(Tag)
    << " "
    << dwarf::ChildrenString(Children)
    << '\n';

  for (unsigned i = 0, N = Data.size(); i < N; ++i) {
    O << "  "
      << dwarf::AttributeString(Data[i].getAttribute())
      << "  "
      << dwarf::FormEncodingString(Data[i].getForm())
      << '\n';
  }
}

LLVM_DUMP_METHOD
void DIEAbbrev::dump() { print(dbgs()); }

DIEAbbrev DIE::generateAbbrev() const {
  DIEAbbrev Abbrev(Tag, hasChildren());
  for (const DIEValue &V : values())
    Abbrev.AddAttribute(V.getAttribute(), V.getForm());
  return Abbrev;
}

/// Climb up the parent chain to get the unit DIE to which this DIE
/// belongs.
const DIE *DIE::getUnit() const {
  const DIE *Cu = getUnitOrNull();
  assert(Cu && "We should not have orphaned DIEs.");
  return Cu;
}

/// Climb up the parent chain to get the unit DIE this DIE belongs
/// to. Return NULL if DIE is not added to an owner yet.
const DIE *DIE::getUnitOrNull() const {
  const DIE *p = this;
  while (p) {
    if (p->getTag() == dwarf::DW_TAG_compile_unit ||
        p->getTag() == dwarf::DW_TAG_type_unit)
      return p;
    p = p->getParent();
  }
  return nullptr;
}

DIEValue DIE::findAttribute(dwarf::Attribute Attribute) const {
  // Iterate through all the attributes until we find the one we're
  // looking for, if we can't find it return NULL.
  for (const auto &V : values())
    if (V.getAttribute() == Attribute)
      return V;
  return DIEValue();
}

LLVM_DUMP_METHOD
static void printValues(raw_ostream &O, const DIEValueList &Values,
                        StringRef Type, unsigned Size, unsigned IndentCount) {
  O << Type << ": Size: " << Size << "\n";

  unsigned I = 0;
  const std::string Indent(IndentCount, ' ');
  for (const auto &V : Values.values()) {
    O << Indent;
    O << "Blk[" << I++ << "]";
    O << "  " << dwarf::FormEncodingString(V.getForm()) << " ";
    V.print(O);
    O << "\n";
  }
}

LLVM_DUMP_METHOD
void DIE::print(raw_ostream &O, unsigned IndentCount) const {
  const std::string Indent(IndentCount, ' ');
  O << Indent << "Die: " << format("0x%lx", (long)(intptr_t) this)
    << ", Offset: " << Offset << ", Size: " << Size << "\n";

  O << Indent << dwarf::TagString(getTag()) << " "
    << dwarf::ChildrenString(hasChildren()) << "\n";

  IndentCount += 2;
  for (const auto &V : values()) {
    O << Indent;
    O << dwarf::AttributeString(V.getAttribute());
    O << "  " << dwarf::FormEncodingString(V.getForm()) << " ";
    V.print(O);
    O << "\n";
  }
  IndentCount -= 2;

  for (const auto &Child : children())
    Child.print(O, IndentCount + 4);

  O << "\n";
}

LLVM_DUMP_METHOD
void DIE::dump() {
  print(dbgs());
}

void DIEValue::EmitValue(const AsmPrinter *AP) const {
  switch (Ty) {
  case isNone:
    llvm_unreachable("Expected valid DIEValue");
#define HANDLE_DIEVALUE(T)                                                     \
  case is##T:                                                                  \
    getDIE##T().EmitValue(AP, Form);                                           \
    break;
#include "llvm/CodeGen/DIEValue.def"
  }
}

unsigned DIEValue::SizeOf(const AsmPrinter *AP) const {
  switch (Ty) {
  case isNone:
    llvm_unreachable("Expected valid DIEValue");
#define HANDLE_DIEVALUE(T)                                                     \
  case is##T:                                                                  \
    return getDIE##T().SizeOf(AP, Form);
#include "llvm/CodeGen/DIEValue.def"
  }
  llvm_unreachable("Unknown DIE kind");
}

LLVM_DUMP_METHOD
void DIEValue::print(raw_ostream &O) const {
  switch (Ty) {
  case isNone:
    llvm_unreachable("Expected valid DIEValue");
#define HANDLE_DIEVALUE(T)                                                     \
  case is##T:                                                                  \
    getDIE##T().print(O);                                                      \
    break;
#include "llvm/CodeGen/DIEValue.def"
  }
}

LLVM_DUMP_METHOD
void DIEValue::dump() const {
  print(dbgs());
}

//===----------------------------------------------------------------------===//
// DIEInteger Implementation
//===----------------------------------------------------------------------===//

/// EmitValue - Emit integer of appropriate size.
///
void DIEInteger::EmitValue(const AsmPrinter *Asm, dwarf::Form Form) const {
  unsigned Size = ~0U;
  switch (Form) {
  case dwarf::DW_FORM_flag_present:
    // Emit something to keep the lines and comments in sync.
    // FIXME: Is there a better way to do this?
    Asm->OutStreamer->AddBlankLine();
    return;
  case dwarf::DW_FORM_flag:  LLVM_FALLTHROUGH;
  case dwarf::DW_FORM_ref1:  LLVM_FALLTHROUGH;
  case dwarf::DW_FORM_data1: Size = 1; break;
  case dwarf::DW_FORM_ref2:  LLVM_FALLTHROUGH;
  case dwarf::DW_FORM_data2: Size = 2; break;
  case dwarf::DW_FORM_sec_offset: LLVM_FALLTHROUGH;
  case dwarf::DW_FORM_strp:  LLVM_FALLTHROUGH;
  case dwarf::DW_FORM_ref4:  LLVM_FALLTHROUGH;
  case dwarf::DW_FORM_data4: Size = 4; break;
  case dwarf::DW_FORM_ref8:  LLVM_FALLTHROUGH;
  case dwarf::DW_FORM_ref_sig8:  LLVM_FALLTHROUGH;
  case dwarf::DW_FORM_data8: Size = 8; break;
  case dwarf::DW_FORM_GNU_str_index: Asm->EmitULEB128(Integer); return;
  case dwarf::DW_FORM_GNU_addr_index: Asm->EmitULEB128(Integer); return;
  case dwarf::DW_FORM_udata: Asm->EmitULEB128(Integer); return;
  case dwarf::DW_FORM_sdata: Asm->EmitSLEB128(Integer); return;
  case dwarf::DW_FORM_addr:
    Size = Asm->getPointerSize();
    break;
  case dwarf::DW_FORM_ref_addr:
    Size = SizeOf(Asm, dwarf::DW_FORM_ref_addr);
    break;
  default: llvm_unreachable("DIE Value form not supported yet");
  }
  Asm->OutStreamer->EmitIntValue(Integer, Size);
}

/// SizeOf - Determine size of integer value in bytes.
///
unsigned DIEInteger::SizeOf(const AsmPrinter *AP, dwarf::Form Form) const {
  switch (Form) {
  case dwarf::DW_FORM_flag_present: return 0;
  case dwarf::DW_FORM_flag:  LLVM_FALLTHROUGH;
  case dwarf::DW_FORM_ref1:  LLVM_FALLTHROUGH;
  case dwarf::DW_FORM_data1: return sizeof(int8_t);
  case dwarf::DW_FORM_ref2:  LLVM_FALLTHROUGH;
  case dwarf::DW_FORM_data2: return sizeof(int16_t);
  case dwarf::DW_FORM_sec_offset: LLVM_FALLTHROUGH;
  case dwarf::DW_FORM_strp:  LLVM_FALLTHROUGH;
  case dwarf::DW_FORM_ref4:  LLVM_FALLTHROUGH;
  case dwarf::DW_FORM_data4: return sizeof(int32_t);
  case dwarf::DW_FORM_ref8:  LLVM_FALLTHROUGH;
  case dwarf::DW_FORM_ref_sig8:  LLVM_FALLTHROUGH;
  case dwarf::DW_FORM_data8: return sizeof(int64_t);
  case dwarf::DW_FORM_GNU_str_index: return getULEB128Size(Integer);
  case dwarf::DW_FORM_GNU_addr_index: return getULEB128Size(Integer);
  case dwarf::DW_FORM_udata: return getULEB128Size(Integer);
  case dwarf::DW_FORM_sdata: return getSLEB128Size(Integer);
  case dwarf::DW_FORM_addr:
    return AP->getPointerSize();
  case dwarf::DW_FORM_ref_addr:
    if (AP->OutStreamer->getContext().getDwarfVersion() == 2)
      return AP->getPointerSize();
    return sizeof(int32_t);
  default: llvm_unreachable("DIE Value form not supported yet");
  }
}

LLVM_DUMP_METHOD
void DIEInteger::print(raw_ostream &O) const {
  O << "Int: " << (int64_t)Integer << "  0x";
  O.write_hex(Integer);
}

//===----------------------------------------------------------------------===//
// DIEExpr Implementation
//===----------------------------------------------------------------------===//

/// EmitValue - Emit expression value.
///
void DIEExpr::EmitValue(const AsmPrinter *AP, dwarf::Form Form) const {
  AP->OutStreamer->EmitValue(Expr, SizeOf(AP, Form));
}

/// SizeOf - Determine size of expression value in bytes.
///
unsigned DIEExpr::SizeOf(const AsmPrinter *AP, dwarf::Form Form) const {
  if (Form == dwarf::DW_FORM_data4) return 4;
  if (Form == dwarf::DW_FORM_sec_offset) return 4;
  if (Form == dwarf::DW_FORM_strp) return 4;
  return AP->getPointerSize();
}

LLVM_DUMP_METHOD
void DIEExpr::print(raw_ostream &O) const { O << "Expr: " << *Expr; }

//===----------------------------------------------------------------------===//
// DIELabel Implementation
//===----------------------------------------------------------------------===//

/// EmitValue - Emit label value.
///
void DIELabel::EmitValue(const AsmPrinter *AP, dwarf::Form Form) const {
  AP->EmitLabelReference(Label, SizeOf(AP, Form),
                         Form == dwarf::DW_FORM_strp ||
                             Form == dwarf::DW_FORM_sec_offset ||
                             Form == dwarf::DW_FORM_ref_addr);
}

/// SizeOf - Determine size of label value in bytes.
///
unsigned DIELabel::SizeOf(const AsmPrinter *AP, dwarf::Form Form) const {
  if (Form == dwarf::DW_FORM_data4) return 4;
  if (Form == dwarf::DW_FORM_sec_offset) return 4;
  if (Form == dwarf::DW_FORM_strp) return 4;
  return AP->getPointerSize();
}

LLVM_DUMP_METHOD
void DIELabel::print(raw_ostream &O) const { O << "Lbl: " << Label->getName(); }

//===----------------------------------------------------------------------===//
// DIEDelta Implementation
//===----------------------------------------------------------------------===//

/// EmitValue - Emit delta value.
///
void DIEDelta::EmitValue(const AsmPrinter *AP, dwarf::Form Form) const {
  AP->EmitLabelDifference(LabelHi, LabelLo, SizeOf(AP, Form));
}

/// SizeOf - Determine size of delta value in bytes.
///
unsigned DIEDelta::SizeOf(const AsmPrinter *AP, dwarf::Form Form) const {
  if (Form == dwarf::DW_FORM_data4) return 4;
  if (Form == dwarf::DW_FORM_sec_offset) return 4;
  if (Form == dwarf::DW_FORM_strp) return 4;
  return AP->getPointerSize();
}

LLVM_DUMP_METHOD
void DIEDelta::print(raw_ostream &O) const {
  O << "Del: " << LabelHi->getName() << "-" << LabelLo->getName();
}

//===----------------------------------------------------------------------===//
// DIEString Implementation
//===----------------------------------------------------------------------===//

/// EmitValue - Emit string value.
///
void DIEString::EmitValue(const AsmPrinter *AP, dwarf::Form Form) const {
  assert(
      (Form == dwarf::DW_FORM_strp || Form == dwarf::DW_FORM_GNU_str_index) &&
      "Expected valid string form");

  // Index of string in symbol table.
  if (Form == dwarf::DW_FORM_GNU_str_index) {
    DIEInteger(S.getIndex()).EmitValue(AP, Form);
    return;
  }

  // Relocatable symbol.
  assert(Form == dwarf::DW_FORM_strp);
  if (AP->MAI->doesDwarfUseRelocationsAcrossSections()) {
    DIELabel(S.getSymbol()).EmitValue(AP, Form);
    return;
  }

  // Offset into symbol table.
  DIEInteger(S.getOffset()).EmitValue(AP, Form);
}

/// SizeOf - Determine size of delta value in bytes.
///
unsigned DIEString::SizeOf(const AsmPrinter *AP, dwarf::Form Form) const {
  assert(
      (Form == dwarf::DW_FORM_strp || Form == dwarf::DW_FORM_GNU_str_index) &&
      "Expected valid string form");

  // Index of string in symbol table.
  if (Form == dwarf::DW_FORM_GNU_str_index)
    return DIEInteger(S.getIndex()).SizeOf(AP, Form);

  // Relocatable symbol.
  if (AP->MAI->doesDwarfUseRelocationsAcrossSections())
    return DIELabel(S.getSymbol()).SizeOf(AP, Form);

  // Offset into symbol table.
  return DIEInteger(S.getOffset()).SizeOf(AP, Form);
}

LLVM_DUMP_METHOD
void DIEString::print(raw_ostream &O) const {
  O << "String: " << S.getString();
}

//===----------------------------------------------------------------------===//
// DIEEntry Implementation
//===----------------------------------------------------------------------===//

/// EmitValue - Emit debug information entry offset.
///
void DIEEntry::EmitValue(const AsmPrinter *AP, dwarf::Form Form) const {

  if (Form == dwarf::DW_FORM_ref_addr) {
    const DwarfDebug *DD = AP->getDwarfDebug();
    unsigned Addr = Entry->getOffset();
    assert(!DD->useSplitDwarf() && "TODO: dwo files can't have relocations.");
    // For DW_FORM_ref_addr, output the offset from beginning of debug info
    // section. Entry->getOffset() returns the offset from start of the
    // compile unit.
    DwarfCompileUnit *CU = DD->lookupUnit(Entry->getUnit());
    assert(CU && "CUDie should belong to a CU.");
    Addr += CU->getDebugInfoOffset();
    if (AP->MAI->doesDwarfUseRelocationsAcrossSections())
      AP->EmitLabelPlusOffset(CU->getSectionSym(), Addr,
                              DIEEntry::getRefAddrSize(AP));
    else
      AP->OutStreamer->EmitIntValue(Addr, DIEEntry::getRefAddrSize(AP));
  } else
    AP->EmitInt32(Entry->getOffset());
}

unsigned DIEEntry::getRefAddrSize(const AsmPrinter *AP) {
  // DWARF4: References that use the attribute form DW_FORM_ref_addr are
  // specified to be four bytes in the DWARF 32-bit format and eight bytes
  // in the DWARF 64-bit format, while DWARF Version 2 specifies that such
  // references have the same size as an address on the target system.
  const DwarfDebug *DD = AP->getDwarfDebug();
  assert(DD && "Expected Dwarf Debug info to be available");
  if (DD->getDwarfVersion() == 2)
    return AP->getPointerSize();
  return sizeof(int32_t);
}

LLVM_DUMP_METHOD
void DIEEntry::print(raw_ostream &O) const {
  O << format("Die: 0x%lx", (long)(intptr_t)&Entry);
}

//===----------------------------------------------------------------------===//
// DIELoc Implementation
//===----------------------------------------------------------------------===//

/// ComputeSize - calculate the size of the location expression.
///
unsigned DIELoc::ComputeSize(const AsmPrinter *AP) const {
  if (!Size) {
    for (const auto &V : values())
      Size += V.SizeOf(AP);
  }

  return Size;
}

/// EmitValue - Emit location data.
///
void DIELoc::EmitValue(const AsmPrinter *Asm, dwarf::Form Form) const {
  switch (Form) {
  default: llvm_unreachable("Improper form for block");
  case dwarf::DW_FORM_block1: Asm->EmitInt8(Size);    break;
  case dwarf::DW_FORM_block2: Asm->EmitInt16(Size);   break;
  case dwarf::DW_FORM_block4: Asm->EmitInt32(Size);   break;
  case dwarf::DW_FORM_block:
  case dwarf::DW_FORM_exprloc:
    Asm->EmitULEB128(Size); break;
  }

  for (const auto &V : values())
    V.EmitValue(Asm);
}

/// SizeOf - Determine size of location data in bytes.
///
unsigned DIELoc::SizeOf(const AsmPrinter *AP, dwarf::Form Form) const {
  switch (Form) {
  case dwarf::DW_FORM_block1: return Size + sizeof(int8_t);
  case dwarf::DW_FORM_block2: return Size + sizeof(int16_t);
  case dwarf::DW_FORM_block4: return Size + sizeof(int32_t);
  case dwarf::DW_FORM_block:
  case dwarf::DW_FORM_exprloc:
    return Size + getULEB128Size(Size);
  default: llvm_unreachable("Improper form for block");
  }
}

LLVM_DUMP_METHOD
void DIELoc::print(raw_ostream &O) const {
  printValues(O, *this, "ExprLoc", Size, 5);
}

//===----------------------------------------------------------------------===//
// DIEBlock Implementation
//===----------------------------------------------------------------------===//

/// ComputeSize - calculate the size of the block.
///
unsigned DIEBlock::ComputeSize(const AsmPrinter *AP) const {
  if (!Size) {
    for (const auto &V : values())
      Size += V.SizeOf(AP);
  }

  return Size;
}

/// EmitValue - Emit block data.
///
void DIEBlock::EmitValue(const AsmPrinter *Asm, dwarf::Form Form) const {
  switch (Form) {
  default: llvm_unreachable("Improper form for block");
  case dwarf::DW_FORM_block1: Asm->EmitInt8(Size);    break;
  case dwarf::DW_FORM_block2: Asm->EmitInt16(Size);   break;
  case dwarf::DW_FORM_block4: Asm->EmitInt32(Size);   break;
  case dwarf::DW_FORM_block:  Asm->EmitULEB128(Size); break;
  }

  for (const auto &V : values())
    V.EmitValue(Asm);
}

/// SizeOf - Determine size of block data in bytes.
///
unsigned DIEBlock::SizeOf(const AsmPrinter *AP, dwarf::Form Form) const {
  switch (Form) {
  case dwarf::DW_FORM_block1: return Size + sizeof(int8_t);
  case dwarf::DW_FORM_block2: return Size + sizeof(int16_t);
  case dwarf::DW_FORM_block4: return Size + sizeof(int32_t);
  case dwarf::DW_FORM_block:  return Size + getULEB128Size(Size);
  default: llvm_unreachable("Improper form for block");
  }
}

LLVM_DUMP_METHOD
void DIEBlock::print(raw_ostream &O) const {
  printValues(O, *this, "Blk", Size, 5);
}

//===----------------------------------------------------------------------===//
// DIELocList Implementation
//===----------------------------------------------------------------------===//

unsigned DIELocList::SizeOf(const AsmPrinter *AP, dwarf::Form Form) const {
  if (Form == dwarf::DW_FORM_data4)
    return 4;
  if (Form == dwarf::DW_FORM_sec_offset)
    return 4;
  return AP->getPointerSize();
}

/// EmitValue - Emit label value.
///
void DIELocList::EmitValue(const AsmPrinter *AP, dwarf::Form Form) const {
  DwarfDebug *DD = AP->getDwarfDebug();
  MCSymbol *Label = DD->getDebugLocs().getList(Index).Label;
  AP->emitDwarfSymbolReference(Label, /*ForceOffset*/ DD->useSplitDwarf());
}

LLVM_DUMP_METHOD
void DIELocList::print(raw_ostream &O) const { O << "LocList: " << Index; }

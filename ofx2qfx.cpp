#include <QFile>
#include <QDebug>
#include <QFileDialog>
#include <QApplication>
#include <QMessageBox>
#include <QDomDocument>
#include <QTextStream>

static const auto kPfcuString = QString("PENTAGON FEDERAL CREDIT UNION");
static const auto kPfcuFid = QString("10360");

QString NodeToString(const QDomNode& node) {
  QString out_string;
  QTextStream stream(&out_string);
  node.save(stream, QDomNode::EncodingFromDocument);
  return out_string;
}

void CheckNodeForError(const QDomNode& node) {
  if (node.isNull()) {
    const auto msg = QString("Error reading node %1").arg(node.nodeName());
    QWidget w;
    QMessageBox::critical(&w, "Node Error", msg);
    exit(-1);
  }
}

void DisplayErrorAndExit(const QString& title, const QString& msg) {
  QWidget w;
  QMessageBox::critical(&w, title, msg);
  exit(-1);
}

// opens and seperates OFX file into xml and non-xml parts
void ReadOfxFile(const QString& filename, QString* non_xml_string, QString* xml_string) {
  // open file
  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    DisplayErrorAndExit("Unable to open file for reading", file.errorString());
  }

  // read file and parse into non-xml and xml strings
  QTextStream out(&file);
  const auto entire_file = out.readAll();
  const auto lines = entire_file.split("\n");
  for (const auto& line : lines) {
    if (!line.contains("<") || !line.contains(">")) { // non-xml
      non_xml_string->append(line);
      non_xml_string->append("\n"); // reinsert newline removed by split
    } else { // xml
      xml_string->append(line);
      xml_string->append("\n"); // reinsert newline removed by split
    }
  }

  // remove last newline in both strings
  xml_string->resize(xml_string->size()-1);
  non_xml_string->resize(non_xml_string->size()-1);

  // close file
  file.close();
}

void WriteQfxFile(const QString& output_filename, const QString& non_xml_string, const QString& xml_string) {
  // open output file for writing
  QFile out_file(output_filename);
  if (!out_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    DisplayErrorAndExit("Unable to open file for writing", out_file.errorString());
  }

  // write file
  QTextStream out(&out_file);
  out << non_xml_string;
  out << xml_string;

  out_file.close();
}

// converts ofx to qfx
int main(int argc, char **argv) {
  QApplication a(argc, argv);
  QWidget w;

  // prompt user for OFX file
  const auto ofx_filename = QFileDialog::getOpenFileName(&w, "Select OFX File", "", "*.OFX");
  if (ofx_filename.isEmpty()) {
    DisplayErrorAndExit("OFX File Selection", "No file selected");
  }

  // read OFX file and parse into xml and non-xml components
  QString non_xml_string;
  QString xml_string;
  ReadOfxFile(ofx_filename, &non_xml_string, &xml_string);
  //qDebug("%s", qPrintable(xml_string));

  // process xml
  QDomDocument document;
  QString error_message;
  int error_line;
  int error_column;
  if (!document.setContent(xml_string, false, &error_message, &error_line, &error_column) ) {
    const auto title = QString("Error reading file %1").arg(ofx_filename);
    const auto msg = QString("%1; line %2, coulumn %3").arg(error_message).arg(error_line).arg(error_column);
    QMessageBox::critical(&w, title, msg);
    return -1;
  }
  //qDebug("new: %s", qPrintable(document.toString()));

  auto root_element = document.documentElement();
  auto signon_node = root_element.namedItem("SIGNONMSGSRSV1"); CheckNodeForError(signon_node);
  auto sonrs_node = signon_node.namedItem("SONRS"); CheckNodeForError(sonrs_node);
  auto fi_node = sonrs_node.namedItem("FI"); CheckNodeForError(fi_node);
  auto org_node = fi_node.namedItem("ORG"); CheckNodeForError(org_node);
  auto fid_node = fi_node.namedItem("FID"); CheckNodeForError(org_node);

  // replace <ORG>Pfcu</ORG> with <ORG>PENTAGON FEDERAL CREDIT UNION</ORG>
  if (org_node.firstChild().nodeValue().isEmpty()) {
    org_node.appendChild(document.createTextNode(kPfcuString));
  } else {
    org_node.firstChild().setNodeValue(kPfcuString);
  }

  // replace <FID></FID> with <FID>10360</FID>
  if (fid_node.firstChild().nodeValue().isEmpty()) {
    fid_node.appendChild(document.createTextNode(kPfcuFid));
  } else {
    fid_node.firstChild().setNodeValue(kPfcuFid);
  }

  // add <INTU.BID>10360</INTU.BID> to <SONRS> node
  auto intu_bid_node = sonrs_node.appendChild(document.createElement("INTU.BID"));
  intu_bid_node.appendChild(document.createTextNode(kPfcuFid));

  // add <INTU.USERID>10360</INTU.USERID> to <SONRS> node
  auto intu_userid_node = sonrs_node.appendChild(document.createElement("INTU.USERID"));
  intu_userid_node.appendChild(document.createTextNode(kPfcuFid));

  qDebug("%s", qPrintable(NodeToString(signon_node)));

  // write out converted OFX xml file
  QFileInfo info(ofx_filename);
  const auto qfx_filename = info.path() + "/" + info.completeBaseName() + ".QFX";
  WriteQfxFile(qfx_filename, non_xml_string, document.toString());

  const auto msg = QString("Successfully converted %1 to OFX format").arg(ofx_filename);
  QMessageBox::information(&w, "Successful Conversion", msg);

  return 0;
}

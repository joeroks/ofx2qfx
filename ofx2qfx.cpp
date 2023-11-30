#include <QFile>
#include <QDebug>
#include <QFileDialog>
#include <QApplication>
#include <QMessageBox>
#include <QDomDocument>
#include <QTextStream>
#include <QStandardPaths>

static const auto kPfcuString = QString("PENTAGON FEDERAL CREDIT UNION");
static const auto kPfcuFid = QString("10360");

QString NodeToString(const QDomNode& node) {
  QString out_string;
  QTextStream stream(&out_string);
  node.save(stream, QDomNode::EncodingFromDocument);
  return out_string;
}

void DisplayErrorAndExit(const QString& title, const QString& msg) {
  QWidget w;
  QMessageBox::critical(&w, title, msg);
  exit(-1);
}

void CheckNodeForError(const QDomNode& node) {
  if (node.isNull()) {
    const auto title = QString( "Node Error");
    const auto msg = QString("Error reading node %1").arg(node.nodeName());
    DisplayErrorAndExit(title, msg);
  }
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
    DisplayErrorAndExit(QString("Unable to open file %1 for writing").arg(output_filename), out_file.errorString());
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

  // prompt user for OFX file. Default to user's download directory
  const auto download_pathname = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
  const auto filter = QString("*.OFX");
  const auto ofx_filename = QFileDialog::getOpenFileName(&w, "Select OFX File", download_pathname, filter);
  if (ofx_filename.isEmpty()) {
    return 0; // no file chosed; just exit
  }

  // read OFX file and parse into xml and non-xml components
  QString non_xml_string;
  QString xml_string;
  ReadOfxFile(ofx_filename, &non_xml_string, &xml_string);

  // process xml
  QDomDocument document;
  QString error_message;
  int error_line;
  int error_column;
  if (!document.setContent(xml_string, false, &error_message, &error_line, &error_column) ) {
    const auto title = QString("Error reading file %1").arg(ofx_filename);
    const auto msg = QString("%1; line %2, coulumn %3").arg(error_message).arg(error_line).arg(error_column);
    DisplayErrorAndExit(title, msg);
  }

  // format of OFX file before conversion:
  // <OFX>
  //    <SIGNONMSGSRSV1>
  //     <SONRS>
  //        <STATUS>
  //          <CODE>0</CODE>
  //          <SEVERITY>INFO</SEVERITY>
  //        </STATUS>
  //        <DTSERVER>20231129204103[0:GMT]</DTSERVER>
  //        <LANGUAGE>ENG</LANGUAGE>
  //        <FI>
  //          <ORG>Pfcu</ORG>
  //          <FID></FID>
  //        </FI>
  //      </SONRS>
  //    </SIGNONMSGSRSV1>
  // .
  // .
  // .
  // </OFX>

  // format of QFX file after conversion:
  // <OFX>
  //    <SIGNONMSGSRSV1>
  //     <SONRS>
  //        <STATUS>
  //          <CODE>0</CODE>
  //          <SEVERITY>INFO</SEVERITY>
  //        </STATUS>
  //        <DTSERVER>20231129204103[0:GMT]</DTSERVER>
  //        <LANGUAGE>ENG</LANGUAGE>
  //        <FI>
  //          <ORG>PENTAGON FEDERAL CREDIT UNION</ORG>
  //          <FID>10360</FID>
  //        </FI>
  //        <INTU.BID>10360</INTU.BID>
  //        <INTU.USERID>10360</INTU.USERID>
  //      </SONRS>
  //    </SIGNONMSGSRSV1>
  // .
  // .
  // .
  // </OFX>

  auto root_element = document.documentElement(); // <OFX>
  auto signon_node = root_element.namedItem("SIGNONMSGSRSV1"); CheckNodeForError(signon_node);
  auto sonrs_node = signon_node.namedItem("SONRS"); CheckNodeForError(sonrs_node);
  auto fi_node = sonrs_node.namedItem("FI"); CheckNodeForError(fi_node);
  auto org_node = fi_node.namedItem("ORG"); CheckNodeForError(org_node);
  auto fid_node = fi_node.namedItem("FID"); CheckNodeForError(org_node);

  // 1. replace <ORG>Pfcu</ORG> with <ORG>PENTAGON FEDERAL CREDIT UNION</ORG>
  if (org_node.firstChild().nodeValue().isEmpty()) {
    org_node.appendChild(document.createTextNode(kPfcuString));
  } else {
    org_node.firstChild().setNodeValue(kPfcuString);
  }

  // 2. replace <FID></FID> with <FID>10360</FID>
  if (fid_node.firstChild().nodeValue().isEmpty()) {
    fid_node.appendChild(document.createTextNode(kPfcuFid));
  } else {
    fid_node.firstChild().setNodeValue(kPfcuFid);
  }

  // 3. add <INTU.BID>10360</INTU.BID> to <SONRS> node
  auto intu_bid_node = sonrs_node.appendChild(document.createElement("INTU.BID"));
  intu_bid_node.appendChild(document.createTextNode(kPfcuFid));

  // 4. add <INTU.USERID>10360</INTU.USERID> to <SONRS> node
  auto intu_userid_node = sonrs_node.appendChild(document.createElement("INTU.USERID"));
  intu_userid_node.appendChild(document.createTextNode(kPfcuFid));

  qDebug("%s", qPrintable(NodeToString(signon_node)));

  // write out converted file
  QFileInfo info(ofx_filename);
  const auto qfx_filename = info.path() + "/" + info.completeBaseName() + ".QFX";
  WriteQfxFile(qfx_filename, non_xml_string, document.toString());

  // tell operator the status
  const auto msg = QString("Successfully converted \n\t%1 \nto \n\t%2")
      .arg(ofx_filename)
      .arg(qfx_filename);
  QMessageBox::information(&w, "Successful Conversion", msg);

  return 0;
}

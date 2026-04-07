#include "lego/pickabrick.h"

#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qjsonarray.h>
#include <QBuffer>

#include "bricklink/core.h"

using namespace Lego::PickABrick;

QByteArray Lego::PickABrick::toLegoPickABrickCSV(const BrickLink::LotList& lots)
{
	QString csv = u"elementId,quantity\n"_qs;

	for ( const BrickLink::Lot* lot : lots) {
		const BrickLink::Item::PCC* pcc = find_pcc(lot->item(), lot->color());
		if (pcc == nullptr) {
			continue;
		}

		csv +=
				QString::number(pcc->pcc()) 
				+ u","_qs
				+ QString::number(lot->quantity())
				+ u"\n"_qs;
		
	}

	return csv.toUtf8();
}


QByteArray Lego::PickABrick::toLegoPickABrickJSON(const BrickLink::LotList& lots)
{
	QJsonArray json;
	for ( const BrickLink::Lot* lot : lots) {
		const BrickLink::Item::PCC* pcc = find_pcc(lot->item(), lot->color());
		if (pcc == nullptr) {
			continue;
		}

		QJsonObject currentItem;
		currentItem.insert(u"elementId"_qs, QString::number(pcc->pcc()) );
		currentItem.insert(u"quantity"_qs, lot->quantity());

		json.append(currentItem);
	}

	QJsonDocument jsonDoc(json);
	return jsonDoc.toJson();
}


BrickLink::IO::ParseResult Lego::PickABrick::fromPickABrickCSV(
	const QByteArray& csv,
	BrickLink::IO::Hint,
	const QDateTime& creationTime
) {
	QByteArray* csv_p = (QByteArray*) & csv;
	QBuffer csv_buffer(csv_p);
	csv_buffer.open(QIODeviceBase::ReadOnly);

	QList<QByteArrayList> content;
	while (!csv_buffer.atEnd()) {
		QByteArray line = csv_buffer.readLine();
		content.append(line.split(','));
	}

	BrickLink::IO::ParseResult parseResult;
	for ( const QByteArrayList& line : content ) {
		if (line.count() < 2) {
			continue;
		}
		bool isPCCvalid = false;
		uint pcc = line[0].toUInt(&isPCCvalid);
		bool isQuantityValid = false;
		uint quantity = line[1].toUInt(&isQuantityValid);
		if (!isQuantityValid || !isPCCvalid) {
			continue;
		}

		parseResult.addLot(buildLot(pcc, quantity, creationTime));
	}

	return parseResult;
}

BrickLink::IO::ParseResult Lego::PickABrick::fromPickABrickJSON(
	const QByteArray& json, 
	BrickLink::IO::Hint,
	const QDateTime& creationTime
) {
	BrickLink::IO::ParseResult parseResult;

	QJsonParseError parseError;
	QJsonDocument jsonDoc = QJsonDocument::fromJson(json, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		return parseResult;
	}


	QJsonArray jsonElements = jsonDoc.array();
	for ( const QJsonValue& jsonVal : jsonElements) {
		if (!jsonVal.isObject()) {
			continue;
		}
		QJsonObject jsonObj = jsonVal.toObject();
		bool isPccValid = false;
		uint pcc = jsonObj.value(u"elementId"_qs).toString().toUInt(&isPccValid);
		int quantity = jsonObj.value(u"quantity"_qs).toInt();

		if (!isPccValid) {
			continue;
		}

		parseResult.addLot(buildLot(pcc, quantity, creationTime));
	}


	return parseResult;
}


const BrickLink::Item::PCC* Lego::PickABrick::find_pcc(
	const BrickLink::Item* item,
	const BrickLink::Color* color)
{
	for (const BrickLink::Item::PCC& pcc : item->pccs()) {
		if (pcc.color() == color) {
			return &pcc;
		}
	}

	return nullptr;
}


BrickLink::Lot* Lego::PickABrick::buildLot(
	uint pcc,
	int quantity,
	const QDateTime& creationTime
) {
	BrickLink::Lot* lot = new BrickLink::Lot();

	auto itemAndColor = BrickLink::core()->findItemAndColorFromPCC(pcc);
	if (itemAndColor.first == nullptr || itemAndColor.second == nullptr) {
		return lot;
	}

	BrickLink::Incomplete* inc = new BrickLink::Incomplete();
	inc->m_color_id = 0;
	inc->m_category_id = 0;
	lot->setIncomplete(inc);
	lot->isIncomplete()->m_item_id = itemAndColor.first->id();
	lot->isIncomplete()->m_color_id = itemAndColor.second->id();
	lot->isIncomplete()->m_itemtype_id = BrickLink::ItemType::idFromFirstCharInString(u"P"_qs);

	BrickLink::core()->resolveIncomplete(lot, 0, creationTime);
	lot->setQuantity(quantity);
	return lot;
}
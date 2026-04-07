#include "lego/pickabrick.h"

#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qjsonarray.h>


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
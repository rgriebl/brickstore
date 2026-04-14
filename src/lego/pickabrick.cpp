#include "lego/pickabrick.h"

#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qjsonarray.h>
#include <QBuffer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>

#include "bricklink/core.h"


namespace Lego::PickABrick {

	Lot::Lot(uint pcc, uint quantity) :
		m_pcc(pcc),
		m_quantity(quantity)
	{
	}


	QByteArray toLegoPickABrickCSV(const QList<Lot>& lots)
	{
		QString csv = u"elementId,quantity\n"_qs;

		for (const Lot& lot : lots) {
			csv +=
				QString::number(lot.m_pcc)
				+ u","_qs
				+ QString::number(lot.m_quantity)
				+ u"\n"_qs;
		}

		return csv.toUtf8();
	}


	QByteArray toLegoPickABrickJSON(const QList<Lot>& lots)
	{
		QJsonArray json;
		for (const Lot& lot : lots) {

			QJsonObject currentItem;
			currentItem.insert(u"elementId"_qs, QString::number(lot.m_pcc));
			currentItem.insert(u"quantity"_qs, QString::number(lot.m_quantity));

			json.append(currentItem);
		}

		QJsonDocument jsonDoc(json);
		return jsonDoc.toJson();
	}


	Lot bricklinkToLegoLot(const BrickLink::Lot& lot)
	{
		const BrickLink::Item::PCC* pcc = queryPCC(lot.item(), lot.color());
		if (pcc == nullptr) {
			return Lot(0, 0);
		}
		return Lot(pcc->pcc(), lot.quantity());
	}


	BrickLink::IO::ParseResult fromPickABrickCSV(
		const QByteArray& csv,
		BrickLink::IO::Hint,
		const QDateTime& creationTime
	) {
		QByteArray* csv_p = (QByteArray*)&csv;
		QBuffer csv_buffer(csv_p);
		csv_buffer.open(QIODeviceBase::ReadOnly);

		QList<QByteArrayList> content;
		while (!csv_buffer.atEnd()) {
			QByteArray line = csv_buffer.readLine();
			content.append(line.split(','));
		}

		BrickLink::IO::ParseResult parseResult;
		for (const QByteArrayList& line : content) {
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

	BrickLink::IO::ParseResult fromPickABrickJSON(
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
		for (const QJsonValue& jsonVal : jsonElements) {
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


	const BrickLink::Item::PCC* guessPCC(
		const BrickLink::Item* item,
		const BrickLink::Color* color
	) {
		if (item == nullptr || color == nullptr) {
			return nullptr;
		}

		const uint itemId = item->id().toUInt();
		const BrickLink::Item::PCC* selectedPCC = nullptr;

		for (const BrickLink::Item::PCC& pcc : item->pccs()) {
			if (pcc.color() != color) {
				continue;
			}

			// if the pcc uses the old naming convention, select it only if it's the only one
			bool isPCColdConvention = pcc.pcc() / 100 == itemId;
			if (isPCColdConvention && selectedPCC == nullptr) {
				selectedPCC = &pcc;
				continue;
			}

			// select the highest pcc with the new convention
			bool isPCCnewConvention = !isPCColdConvention;
			bool isPCChighterThanPrevious = selectedPCC == nullptr || selectedPCC->pcc() < pcc.pcc();
			if (isPCChighterThanPrevious && isPCCnewConvention) {
				selectedPCC = &pcc;
			}
		}

		return selectedPCC;
	}


	const BrickLink::Item::PCC* queryPCC(
		const BrickLink::Item* item,
		const BrickLink::Color* color
	) {
		const BrickLink::Item::PCC* probablePCC = guessPCC(item, color);
		if (checkIfQueryHasResults(QString::number(probablePCC->pcc()))) {
			return probablePCC;
		}

		QList<BrickLink::Item::PCC*> pccCandidates;
		for (const BrickLink::Item::PCC& pcc : item->pccs()) {
			if (pcc.color() != color) {
				continue;
			}

			if (checkIfQueryHasResults(QString::number(pcc.pcc()))) {
				return &pcc;
			}
		}

		return nullptr;
	}


	bool checkIfQueryHasResults(const QString query) {
		const QString elementContainerClassName = u"ElementsContainer_container__"_qs;

		QNetworkAccessManager network;
		network.setTransferTimeout();

		const QString urlBase = u"https://www.lego.com/pick-and-build/pick-a-brick?query="_qs;
		const QString url = urlBase + query;
		QNetworkRequest request(url);

		QNetworkReply* reply = network.get(request);
		QEventLoop waitingLoop;
		QObject::connect(reply, &QNetworkReply::finished, &waitingLoop, &QEventLoop::quit);
		waitingLoop.exec();

		if (reply->error() != QNetworkReply::NoError) {
			return false;
		}

		QString replyContent = QString::fromUtf8(reply->readAll());
		bool item_found = replyContent.contains(elementContainerClassName);

		return item_found;
	}


	BrickLink::Lot* buildLot(
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

} // namespace Lego::PickABrick
#include "pccfetcher.h"

#include <QtConcurrent>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace Lego::PickABrick {


	PCCfetcher::PCCfetcher(const BrickLink::LotList& lots) :
		m_lots(lots),
		m_progressMutex(),
		m_progress(0),
		m_isRunning(false),
		m_future()
	{
	}


	PCCfetcher::~PCCfetcher() {}


	bool PCCfetcher::start() {
		if (m_isRunning) {
			return false;
		}
		m_isRunning = true;
		m_progress = 0;

		m_future = QtConcurrent::run(
			[this]() {return convertLots(m_lots); }
		);

		return true;
	}


	void PCCfetcher::stop() {
		if (!m_isRunning) {
			return;
		}
		
		m_future.cancel();
		m_future = QFuture<QList<Lot>>();
		m_isRunning = false;
		emit updateFinished(false, tr("Operation canceled"));
	}


	bool PCCfetcher::isRunning() const {
		return m_isRunning;
	}


	int PCCfetcher::maxProgress() const {
		return m_lots.size();
	}


	QList<Lot> PCCfetcher::results() const {
		if (!m_future.isFinished()) {
			return QList<Lot>();
		}
		return m_future.result();
	}


	void PCCfetcher::waitForFinished() {
		m_future.waitForFinished();
	}


	Lot PCCfetcher::convertLot(const BrickLink::Lot* lot) {
		Lot result = bricklinkToLegoLot(*lot);

		QMutexLocker locker(&m_progressMutex);
		m_progress++;
		emit updateProgress(m_progress, maxProgress());

		if (m_progress >= maxProgress()) {
			emit updateFinished(true, tr("Conversion finished") );
		}

		return result;
	}


	PCCfetcher::ItemQueries::ItemQueries(const BrickLink::Lot& lot, uint lotIndex) : 
		m_lotIndex(lotIndex),
		m_pccs(),
		m_lot(lot)
	{
		const BrickLink::Item::PCC* bestPCC = guessPCC(lot.item(), lot.color());
		for ( const BrickLink::Item::PCC& pcc : lot.item()->pccs() ) {
			bool isBestPCC = bestPCC != nullptr && &pcc == bestPCC;
			bool isCorrectColor = pcc.color() == lot.color();
			if (!isBestPCC && isCorrectColor) {
				m_pccs.push(pcc.pcc());
			}
		}

		if (bestPCC != nullptr) {
			m_pccs.push(bestPCC->pcc());
		}
	}


	bool containsId(const QString content, const BrickLink::Item* item) {
		if (item == nullptr) {
			return false;
		}

		QString idsRegexPattern = u"("_qs + QString::fromUtf8(item->id()) + u")"_qs;
		if ( item->hasAlternateIds() ) {
			QStringList alternateIds = QString::fromUtf8(item->alternateIds()).split(u' ');
			for ( const QString& alternateId : alternateIds ) {
				idsRegexPattern += u"|("_qs + alternateId + u")"_qs;
			}
		}
		QRegularExpression IdsRegex(idsRegexPattern);
		return content.contains(IdsRegex);
	}


	QList<std::list<PCCfetcher::ItemQueries>::iterator> PCCfetcher::selectQueries(
		std::list<ItemQueries>& queries, uint maxCount ) 
	{
		QList<std::list<ItemQueries>::iterator> selection;
		QSet<const BrickLink::Item*> selectedItems;

		uint count = 0;
		for (auto iter = queries.begin(); iter != queries.end(); iter++ ) {
			if (count >= maxCount) {
				break;
			}
			const BrickLink::Item* currentItem = iter->m_lot.item();
			if (!selectedItems.contains(currentItem)) {
				selection.append(iter);
				selectedItems.insert(currentItem);
			}
		}

		return selection;
	}


	QList<Lot> PCCfetcher::convertLots(const BrickLink::LotList& lots) {

		QList<Lot> results(lots.size(), Lot());

		std::list<ItemQueries> queries;
		for (int i = 0; i < lots.size(); i++) {
			if (lots[i] != nullptr) {
				queries.push_back(ItemQueries(*lots[i], i));
			}
		}

		while (!queries.empty()) {
			
			QList<std::list<ItemQueries>::iterator> queriesSelection = selectQueries(queries, 100);
			QString pabResult;
			try {
				pabResult = queryPCCs(queriesSelection);
			} catch ( const std::runtime_error& ) {
				
				continue;
			}

			for (std::list<ItemQueries>::iterator queryIter : queriesSelection) {
				bool isPCCfound = containsId(pabResult, queryIter->m_lot.item());
				
				if (isPCCfound) {
					results[queryIter->m_lotIndex] = Lot(
						queryIter->m_pccs.top(), 
						queryIter->m_lot.quantity()
					);
				}

				queryIter->m_pccs.pop();
				if (isPCCfound || queryIter->m_pccs.isEmpty()) {
					queries.erase(queryIter);
				}
			}

			emit updateProgress(lots.size() - (int)queries.size(), lots.size());
		}

		emit updateFinished(true, tr("Conversion finished"));
		return results;
	}


	QString PCCfetcher::queryPCCs(const QList<std::list<ItemQueries>::iterator>& queries) {
		if (queries.empty()) {
			return QString();
		}
		
		QNetworkAccessManager network;
		network.setTransferTimeout();

		QString pccString;
		for (std::list<ItemQueries>::iterator queryIter : queries ) {
			const QStack<uint>& PCCs = queryIter->m_pccs;
			if (PCCs.isEmpty()) {
				continue;
			}

			pccString += QString::number(PCCs.top()) + u"+"_qs;
		}
		pccString.removeLast();

		QUrlQuery query;
		query.addQueryItem(u"perPage"_qs, QString::number(100));
		query.addQueryItem(u"query"_qs, pccString);

		QUrl url(u"https://www.lego.com/pick-and-build/pick-a-brick"_qs);
		url.setQuery(query);

		QNetworkRequest request(url);

		QNetworkReply* reply = network.get(request);
		QEventLoop waitingLoop;
		connect(reply, &QNetworkReply::finished, &waitingLoop, &QEventLoop::quit);
		waitingLoop.exec();

		if (reply->error() != QNetworkReply::NoError) {
			throw std::runtime_error(reply->errorString().toStdString());
		}

		return QString::fromUtf8(reply->readAll());
	}


} // namespace Lego::PickABrick
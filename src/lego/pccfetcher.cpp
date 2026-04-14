#include "pccfetcher.h"


#include <QtConcurrent>

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

		m_future = QtConcurrent::mapped(
			m_lots, 
			[this](const BrickLink::Lot* lot) {return convertLot(lot); }
		);

		return true;
	}


	void PCCfetcher::stop() {
		if (!m_isRunning) {
			return;
		}
		
		m_future.cancel();
		m_future = QFuture<Lot>();
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
		return m_future.results();
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

} // namespace Lego::PickABrick
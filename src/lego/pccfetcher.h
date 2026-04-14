#pragma once

#include <QObject>
#include <QFuture>
#include <QStack>

#include "bricklink/lot.h"

#include "pickabrick.h"


namespace Lego::PickABrick {

	/// <summary>
	/// PCC fetcher is a class that converts a batch of Bricklink lots
	/// to Pick a Brick lots. It is designed to be used with UIHelpers::progressDialog
	/// </summary>
	class PCCfetcher : public QObject {
	Q_OBJECT

	signals:
		/// <summary>
		/// signal sent when new lots were converted.
		/// </summary>
		/// <param name="progress">amount of lots converted</param>
		/// <param name="total">total amount of lots in the batch</param>
		void updateProgress(int progress, int total);

		/// <summary>
		/// signal sent when every lot is done.
		/// </summary>
		/// <param name="success">whether the conversion was a success</param>
		/// <param name="message">a message that sums up the results</param>
		void updateFinished(bool success, const QString& message);
	

	public:
		/// <summary>
		/// Build a new instance of PCCfetcher
		/// </summary>
		/// <param name="lots">the lot list to convert</param>
		PCCfetcher( const BrickLink::LotList& lots );

		/// <summary>
		/// Destroy the instance of PCCfetcher
		/// </summary>
		~PCCfetcher();

		/// <summary>
		/// Start the conversion, in new threads.
		/// </summary>
		/// <returns> Whether the process was actually started</returns>
		bool start();

		/// <summary>
		/// Stop the process.
		/// </summary>
		void stop();

		/// <returns>Whether the conversion process is running</returns>
		bool isRunning() const;

		/// <returns>The amount of lots in the batch.</returns>
		int maxProgress() const;

		/// <summary>
		/// Returns the results of the conversion. If called before the conversion is finished,
		/// returns an empty list.
		/// </summary>
		/// <returns>The list of the converted lots</returns>
		QList<Lot> results() const;

		/// <summary>
		/// Wait for the conversion to be finished.
		/// </summary>
		void waitForFinished();

	private:
		const BrickLink::LotList& m_lots;
		QMutex m_progressMutex;
		int m_progress;
		bool m_isRunning;

		QFuture<QList<Lot>> m_future;


		/// <summary>
		/// ItemQueries represents a lot, with a set of pcc to check out
		/// </summary>
		struct ItemQueries {

		public:
			uint m_lotIndex;
			QStack<uint> m_pccs;
			const BrickLink::Lot& m_lot;

			ItemQueries(const BrickLink::Lot& lot, uint lotIndex=0);
		};

		using QueryIterators = QList<std::list<ItemQueries>::iterator>;

		/// <summary>
		/// Convert an individual lot
		/// </summary>
		/// <param name="lot">the Bricklink lot to convert</param>
		/// <returns>the converted lot</returns>
		Lot convertLot( const BrickLink::Lot* lot);


		/// <summary>
		/// Convert a set of lots, by batch. Emits updateProgress and updateFinished signals.
		/// </summary>
		/// <param name="lots">the lot lists to convert</param>
		/// <returns>the converted lots</returns>
		QList<Lot> convertLots(const BrickLink::LotList& lots );


		/// <summary>
		/// Helper function to query a given set of pcc. The function takes a list of
		/// iterators from an std::list of ItemQueries. It makes one http request to
		/// Lego Pick a Brick, that includes the first pcc of every query. The maximum amount
		/// of queries is 100, the queries beyond that won't be included in the first web page.
		/// </summary>
		/// <exception cref="std::runtime_error">Thrown when an error occured during the request</exception>
		/// <param name="queries">A list of queries</param>
		/// <returns>The returned webpage</returns>
		static QString queryPCCs(const QueryIterators& queries);


		/// <summary>
		/// Select queries from a given list to build a proper batch, 
		/// that can be fed to queryPCCS. It ensures there are no more than
		/// maxCount queries in the result, and every item appears only once, 
		/// regardless of the colors.
		/// </summary>
		/// <param name="queries">A list of queries.</param>
		/// <param name="maxCount">The maximum elements count of the result</param>
		/// <returns>A list of iterators, pointings to the queries list.</returns>
		static QueryIterators selectQueries(
			std::list<ItemQueries>& queries, 
			uint maxCount);
	};



}
#pragma once

#include <QtCore/QString>

#include "bricklink/lot.h"
#include "bricklink/io.h"

/// <summary>
/// This namespace contains helper functions to help conversions with files 
/// compatible with the official Lego Pick a Brick service.
/// </summary>
namespace Lego::PickABrick {

	/// <summary>
	/// Represent a lot with the expected informations for Pick a Brick.
	/// </summary>
	struct Lot {
	public:
		Lot(uint pcc=0, uint quantity=0);

		uint m_pcc;
		uint m_quantity;
	};


	/// <summary>
	/// Builds the content of a CSV file, compliant with the Lego Pick a brick standard.
	/// </summary>
	/// <param name="lots">The lots to include in the file</param>
	/// <returns>The CSV content as a QByteArray, using UTF8 encoding</returns>
	QByteArray toLegoPickABrickCSV(const QList<Lot>& lots);


	/// <summary>
	/// Builds the content of a JSON file, compliant with the Lego Pick a brick standard.
	/// </summary>
	/// <param name="lots">The lots to include in the file</param>
	/// <returns>The CSV content as a QByteArray, using UTF8 encoding</returns>
	QByteArray toLegoPickABrickJSON(const QList<Lot>& lots);


	/// <summary>
	/// Convert a lot from the Bricklink to the Pick a Brick format. Makes querries to the
	/// Pick a Brick website to find the correct PCC.
	/// </summary>
	/// <param name="lot">a Bricklink lot</param>
	/// <returns>The lot converted as a Pick a Brick lot. Lot(0, 0) if an error occured.</returns>
	Lot bricklinkToLegoLot(const BrickLink::Lot& lot);


	/// <summary>
	/// Build an inventory from the content of a csv file, compatible with Lego Pick a Brick.
	/// </summary>
	/// <param name="csv">The content of the csv file</param>
	/// <param name="hint"></param>
	/// <param name="creationTime"></param>
	/// <returns>The parse result of the csv content</returns>
	BrickLink::IO::ParseResult fromPickABrickCSV(
		const QByteArray& csv,
		BrickLink::IO::Hint hint, 
		const QDateTime& creationTime
	);


	/// <summary>
	/// Build an inventory from the content of a json file, compatible with Lego Pick a Brick.
	/// </summary>
	/// <param name="csv">The content of the json file</param>
	/// <param name="hint"></param>
	/// <param name="creationTime"></param>
	/// <returns>The parse result of the json content</returns>
	BrickLink::IO::ParseResult fromPickABrickJSON(
		const QByteArray& json,
		BrickLink::IO::Hint hint,
		const QDateTime& creationTime
	);


	/// <summary>
	/// Guess, from the Bricklink local database, what the PCC might be for the given item and color combinaison.
	/// If only one PCC in the old format (part code + color code put together) is found, it is returned.
	/// As long a PCC in the new format (7 digit code, starting with 6 or 4) are there, the highest one is returned.
	/// </summary>
	/// <param name="item">a bricklink item</param>
	/// <param name="color">a bricklink color</param>
	/// <returns>A pointer to the most probable PCC. nullptr if none was found.</returns>
	const BrickLink::Item::PCC* guessPCC(
		const BrickLink::Item* item,
		const BrickLink::Color* color);


	/// <summary>
	/// Makes querries to the Pick a Brick website to find the correct PCC for the given item and color combinaison.
	/// The PCC returned by guessPCC is tested first. If it is not correct, the other ones are then tested.
	/// </summary>
	/// <param name="item">a Bricklink item</param>
	/// <param name="color">a Bricklink color</param>
	/// <returns>A pointer to the correct PCC, availablel in Pick a Brick. nullptr if none was found.</returns>
	const BrickLink::Item::PCC* querryPCC(
		const BrickLink::Item* item,
		const BrickLink::Color* color
	);


	/// <summary>
	/// Makes a request to the Pick a Brick website and return wether the html page contains results.
	/// </summary>
	/// <param name="querry">The item to search for</param>
	/// <returns>Whether the html page contains any results. False if an error occured.</returns>
	bool checkIfQuerryHasResults(const QString querry);


	/// <summary>
	/// Build a bricklink lot from a given pcc and quantity
	/// </summary>
	/// <param name="pcc">a pcc</param>
	/// <param name="quantity">the lot quantity</param>
	/// <param name="creationTime"></param>
	/// <returns>The new lot. Empty of pcc is invalid.</returns>
	BrickLink::Lot* buildLot(
		uint pcc,
		int quantity,
		const QDateTime& creationTime);
	
} // namespace Lego::PickABrick 
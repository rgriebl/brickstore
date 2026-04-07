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
	/// Builds the content of a CSV file, compliant with the Lego Pick a brick standard.
	/// </summary>
	/// <param name="lots">The lots to include in the file</param>
	/// <returns>The CSV content as a QByteArray, using UTF8 encoding</returns>
	QByteArray toLegoPickABrickCSV(const BrickLink::LotList& lots);


	/// <summary>
	/// Builds the content of a JSON file, compliant with the Lego Pick a brick standard.
	/// </summary>
	/// <param name="lots">The lots to include in the file</param>
	/// <returns>The CSV content as a QByteArray, using UTF8 encoding</returns>
	QByteArray toLegoPickABrickJSON(const BrickLink::LotList &lots);


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
	/// Find and return the official pcc (part/color code ?) matching the provided item and color pair.
	/// </summary>
	/// <param name="item">a bricklink item</param>
	/// <param name="color">a bricklink color</param>
	/// <returns>A pointer to the first marching pcc if found, nullptr otherwise.</returns>
	const BrickLink::Item::PCC* find_pcc(
		const BrickLink::Item* item,
		const BrickLink::Color* color);


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
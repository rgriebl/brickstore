#pragma once

#include <QtCore/QString>
#include "bricklink/lot.h"

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
	/// Find and return the official pcc (part/color code ?) matching the provided item and color pair.
	/// </summary>
	/// <param name="item">a bricklink item</param>
	/// <param name="color">a bricklink color</param>
	/// <returns>A pointer to the first marching pcc if found, nullptr otherwise.</returns>
	const BrickLink::Item::PCC* find_pcc(
		const BrickLink::Item* item,
		const BrickLink::Color* color);
	
} // namespace Lego::PickABrick 
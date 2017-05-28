/*
  Remote Temerature Station
  Copyright (C) 2015 by Jason K Jackson jake1164 at hotmail.com
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _HEADER_H
#define _HEADER_H

struct measure {
	int count;
	float sum;
	float minimum;
	float maximum;
};

typedef struct {
  char header;
  bool charge;
  bool done;  
  long lightlevel;
  float humidity;
  float temperature_dht;
  float temperature_bmp;
  float pressure;
  float voltage;
  float dewpoint;

} txMeasureStruct;

void reset(measure &reading);

#endif

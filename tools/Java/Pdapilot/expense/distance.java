/* 
 * expense/distance.java:
 *
 * Copyright (C) 1997, 1998, Kenneth Albanowski
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

package Pdapilot.expense;

public class distance {
	private int idx;
	
	final static private String[] names = 
		{ "Miles", "Kilometers" };

	public static distance Miles = new distance(0);
	public static distance Kilometers =  new distance(1);
	
	private static distance[] objs;
	
	private distance(int value) {
		this.idx = value;
	}
	
	public static distance get(int value) {
		return objs[value];
	}
	public static distance get(String value) {
		int i;
		for(i=0;i<names.length;i++)
			if (names[i].equals(value))
				return objs[i];
		return null;
	}
	public static String[] getNames() {
		return names;
	}
	
	public String toString() {
		return "distance."+names[idx];
	}
		
	public int getValue() {
		return idx;
	}
	public String getName() {
		return names[idx];
	}

	static {
		objs = new distance[2];
		objs[0] = distance.Miles;
		objs[1] = distance.Kilometers;
	}
};
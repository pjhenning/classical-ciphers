open Containers;
open Containers.List.Infix;

type operation_mode = 
  | Encrypt
  | Decrypt;

module Caesar = {
  let shift_letter = (letter, ~by as shift_amount) => {
    let last_letter_code  = 122; /* ASCII code for 'z' */
    let first_letter_code = 97;  /* ASCII code for 'a' */
    let code_ceiling      = last_letter_code + 1;
    let input_code = letter |> Char.lowercase_ascii |> Char.code;

    let shifted_val_temp =
      (input_code + shift_amount) mod code_ceiling;
    let shifted_val =
      (shifted_val_temp < first_letter_code) ?
        (shifted_val_temp + first_letter_code) : shifted_val_temp;

    Char.chr(shifted_val)
  };

  let shift_letters = (input_string, ~by, ~mode) => {
    let by = 
      switch (mode) {
      | Encrypt => by
      | Decrypt => Int.neg(by)
      };
    String.map(shift_letter(~by), input_string)
  };
};

module Playfair = {

  module Char_Map = Core_kernel.Char.Map;
  module Map = Core_kernel.Map;

  type matrix_location = {
    row: int,
    column: int
  };

  type meta_matrix = {
    locations: Char_Map.t(matrix_location),
    indices: Char_Map.t(int),
    array: CCImmutArray.t(char)
  };

  type matrix_alignment = 
    | Column
    | Row
    | None;

  let strip_j = (input_string) => 
    String.replace(~which=`All, ~sub="j", ~by="i", input_string);

  let build_list = key => {
    module Char_Set = Set.Make(Char);
    let alphabet = 
      "abcdefghiklmnopqrstuvwxyz" |> String.to_list |> Char_Set.of_list;
    let key_l = key |> strip_j |> String.to_list;
    let key_s = Char_Set.of_list(key_l);
    let filtered_alphabet = Char_Set.diff(alphabet, key_s) |> Char_Set.to_list;
    List.append(key_l, filtered_alphabet)
  };

  let increment_location = location => {
    switch (location.column) {
    | 4 => {row: location.row + 1, column: 0}
    | _ => {...location, column: location.column + 1}
    }
  };

  let calculate_locations = () => {
    let rec calculate = (cells_remaining, ~previous=?, ()) => {
      switch (cells_remaining) {
      | 0 => []
      | _ => {
          let location = 
            switch (previous) {
            | Some(location) => increment_location(location)
            | None => {row: 0, column: 0}  /* This case only occurs for 1st element */
            };
          [location, ...calculate(cells_remaining - 1, ~previous=location, ())]
        }
      }
    };
    let cell_count = 25;
    calculate(cell_count, ())
  };

  let build_map = letters => {
    let locations = calculate_locations();
    List.combine(letters, locations) |> Char_Map.of_alist_exn
  };

  let rec insert_needed_filler = word_l => {
    switch (word_l) {
    | [first, second, ...rest] =>
      Char.equal(first, second) ?
        [first, 'x', ...insert_needed_filler([second, ...rest])] :
        [first, second, ...insert_needed_filler(rest)]
    | [first] => [first]
    | [] => []
    }
  };

  let get_alignment = (pair, locations) => {
    let a = List.nth(pair, 0);
    let b = List.nth(pair, 1);
    let location_a = Map.find_exn(locations, a);
    let location_b = Map.find_exn(locations, b);
    location_a.column == location_b.column ?
      Column : location_a.row == location_b.row ? Row : None;
  };

  let circular_row_shift = (letter, ~letters, ~mode) => {
    let index = Map.find_exn(letters.indices, letter);
    let new_index = 
      switch (mode) {
      | Encrypt => 
        switch ((index + 1) mod 5) {
        | 0 => index - 4
        | _ => index + 1
        }
      | Decrypt => 
        switch (index mod 5) {
        | 0 => index + 4
        | _ => index - 1
        }
      };
    CCImmutArray.get(letters.array, new_index)
  };

  let circular_column_shift = (letter, ~letters, ~mode) => {
    let index = Map.find_exn(letters.indices, letter);
    let new_index = 
      switch (mode) {
      | Encrypt => 
        switch (index > 19) {
        | true => index - 20
        | false => index + 5
        }
      | Decrypt => 
        switch (index < 5) {
        | true => index + 20
        | false => index - 5
        }
      };
    CCImmutArray.get(letters.array, new_index)
  };

  let location_to_index = location => {
    location.column + (location.row * 5)
  };

  let alternate_replace = (pair, ~letters) => {
    let a = List.nth(pair, 0);
    let b = List.nth(pair, 1);
    let a_loc = Map.find_exn(letters.locations, a);
    let b_loc = Map.find_exn(letters.locations, b);
    let new_a_loc = { row: a_loc.row, column: b_loc.column };
    let new_b_loc = { row: b_loc.row, column: a_loc.column };
    let new_a = new_a_loc |> location_to_index |> CCImmutArray.get(letters.array);
    let new_b = new_b_loc |> location_to_index |> CCImmutArray.get(letters.array);
    [new_a, new_b]
  };

  let substitute_pair = (pair, ~letters, ~mode) => {
    switch (get_alignment(pair, letters.locations)) {
    | Column => [ 
        circular_column_shift(List.nth(pair, 0), ~letters, ~mode),
        circular_column_shift(List.nth(pair, 1), ~letters, ~mode)
      ]
    | Row => [
        circular_row_shift(List.nth(pair, 0), ~letters, ~mode),
        circular_row_shift(List.nth(pair, 1), ~letters, ~mode)
      ]
    | None => alternate_replace(pair, ~letters)
    }
  };

  let substitute = (text, ~key, ~mode) => {
    let proto_matrix = build_list(key);
    let m_matrix = {
      locations: build_map(proto_matrix),
      indices: List.combine(proto_matrix, 0--24) |> Char_Map.of_alist_exn,
      array: CCImmutArray.of_list(proto_matrix)
    };
    let final_plaintext_l = 
      switch (mode) {
      /* TODO: For encrypting, check whether 'filled' list is of even length and pad if not */
      | Encrypt => text |> strip_j |> String.to_list |> insert_needed_filler
      | Decrypt => text |> strip_j |> String.to_list
      };
    let pairs = List.sublists_of_len(2, final_plaintext_l);
    List.map(substitute_pair(~letters=m_matrix, ~mode), pairs) 
    |> List.flatten 
    |> String.of_list
  };

};

let () = Playfair.substitute("bolloon", ~key="monarchy", ~mode=Encrypt) |> print_endline;
